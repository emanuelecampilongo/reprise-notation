/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "abstractnotationpaintview.h"
#include "reprisesmartshapeadapter.h"
#include "reprisesmartshapepreview.h"

#include <QCursor>
#include <QPainter>
#include <QPainterPath>
#include <QLineF>
#include <algorithm>
#include <limits>
#include <optional>
#include <QMimeData>

#include "async/async.h"

#include "actions/actiontypes.h"
#include "internal/notationuiactions.h"
#include "engraving/dom/accidental.h"
#include "engraving/dom/anchors.h"
#include "engraving/dom/shadownote.h"
#include "engraving/dom/note.h"
#include "engraving/dom/pitchspelling.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/hairpin.h"
#include "engraving/dom/score.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/slur.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/system.h"
#include "engraving/dom/utils.h"
#include <utility>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif
#include "log.h"

using namespace mu;
using namespace mu::notation;
using namespace muse;
using namespace muse::ui;
using namespace muse::draw;
using namespace muse::actions;

namespace repriseSmartShape = mu::notation::reprise::smartshape;
namespace repriseSmartShapePreview = mu::notation::reprise::smartshape::preview;

static constexpr qreal SCROLL_LIMIT_OFF_OVERSCROLL_FACTOR = 0.75;
static constexpr qreal REPRISE_NOTE_TARGET_RADIUS_PX = 48.0;
static constexpr qreal REPRISE_NOTE_START_RADIUS_PX = 48.0;

static void compensateFloatPart(RectF& rect)
{
    rect.adjust(-1, -1, 1, 1);
}

AbstractNotationPaintView::AbstractNotationPaintView(QQuickItem* parent)
    : muse::uicomponents::QuickPaintedView(parent), muse::Contextable(muse::iocCtxForQmlObject(this))
{
    setFlag(ItemHasContents, true);
    setFlag(ItemAcceptsDrops, true);
    setAcceptedMouseButtons(Qt::AllButtons);

    connect(this, &QQuickPaintedItem::widthChanged, this, &AbstractNotationPaintView::onViewSizeChanged);
    connect(this, &QQuickPaintedItem::heightChanged, this, &AbstractNotationPaintView::onViewSizeChanged);

    connect(this, &AbstractNotationPaintView::horizontalScrollChanged, [this]() {
        m_previousHorizontalScrollPosition = startHorizontalScrollPosition();
    });

    connect(this, &AbstractNotationPaintView::verticalScrollChanged, [this]() {
        m_previousVerticalScrollPosition = startVerticalScrollPosition();
    });

    m_enableAutoScrollTimer.setSingleShot(true);
    connect(&m_enableAutoScrollTimer, &QTimer::timeout, this, [this]() {
        m_autoScrollEnabled = true;
    });
}

AbstractNotationPaintView::~AbstractNotationPaintView()
{
    clearPlaybackScrubNotes();

    if (m_notation && isMainView()) {
        m_notation->accessibility()->setMapToScreenFunc(nullptr);
        m_notation->interaction()->setGetViewRectFunc(nullptr);
    }

    clear();
}

RepriseToolModel* AbstractNotationPaintView::repriseToolModel() const
{
    return m_repriseToolModel.data();
}

void AbstractNotationPaintView::setRepriseToolModel(RepriseToolModel* model)
{
    if (m_repriseToolModel == model) {
        return;
    }
    cancelRepriseSlur();
    QObject::disconnect(m_repriseToolStateConnection);
    if (m_repriseToolModel) {
        m_repriseToolModel->reset();
    }
    m_repriseToolModel = model;
    if (model) {
        m_repriseToolStateConnection = connect(model, &RepriseToolModel::stateChanged, this, [this]() {
            if (!m_repriseToolModel || !m_repriseToolModel->operationActive()
                || m_repriseToolModel->activeTool() != RepriseToolModel::SmartShape) {
                m_repriseSlurCommit = {};
                m_reprisePointer.cancel();
                clearRepriseSlurPreview();
            }
        });
    }
    m_repriseEscapeHeld = false;
    emit repriseToolModelChanged();
}

void AbstractNotationPaintView::selectRepriseTool(int tool)
{
    if (!m_repriseToolModel || !m_repriseToolModel->enabled() || !isMainView() || publishMode() || m_readonly) {
        return;
    }

    const auto noteInput = notationNoteInput();
    if (noteInput && noteInput->isNoteInputMode()) {
        noteInput->endNoteInput();
    }
    m_repriseToolModel->selectTool(tool);
}

QString AbstractNotationPaintView::repriseViewMode() const
{
    if (!m_notation) {
        return {};
    }
    switch (m_notation->viewMode()) {
    case ViewMode::LINE: return QStringLiteral("view-mode-continuous");
    case ViewMode::PAGE: return QStringLiteral("view-mode-page");
    default: return {};
    }
}

bool AbstractNotationPaintView::requestRepriseViewMode(const QString& actionCode)
{
    if (actionCode != QStringLiteral("view-mode-continuous") && actionCode != QStringLiteral("view-mode-page")) {
        return false;
    }
    if (!isInited() || !m_notation || !isMainView() || m_readonly || publishMode() || !hasActiveFocus()
        || !m_repriseToolModel || !m_repriseToolModel->enabled() || !dispatcher()
        || m_notation != globalContext()->currentNotation() || m_isContextMenuOpen
        || m_currentElementPopupType != PopupModelType::TYPE_UNDEFINED
        || m_inputController->hasNativeEscapePriority(m_reprisePointer.ownsPointer())) {
        return false;
    }
    cancelRepriseSlur();
    dispatcher()->dispatch(codeFromQString(actionCode));
    return true;
}

void AbstractNotationPaintView::load()
{
    TRACEFUNC;

    m_loadCalled = true;
    m_inputController = std::make_unique<NotationViewInputController>(this, iocContext());
    m_playbackCursor = std::make_unique<PlaybackCursor>(iocContext());
    m_playbackCursor->setVisible(false);
    m_noteInputCursor = std::make_unique<NoteInputCursor>(iocContext(), notationConfiguration()->thinNoteInputCursor());
    m_ruler = std::make_unique<NotationRuler>(iocContext());

    m_loopInMarker = std::make_unique<LoopMarker>(LoopBoundaryType::LoopIn, iocContext());
    m_loopOutMarker = std::make_unique<LoopMarker>(LoopBoundaryType::LoopOut, iocContext());

    m_continuousPanel = std::make_unique<ContinuousPanel>(iocContext());

    //! NOTE For diagnostic tools
    if (!dispatcher()->isReg(this)) {
        dispatcher()->reg(this, "diagnostic-notationview-redraw", [this]() {
            scheduleRedraw();
        });
    }

    m_inputController->setReadonly(m_readonly);
    m_inputController->init();

    onNotationSetup();

    initBackground();
    initNavigatorOrientation();

    configuration()->isLimitCanvasScrollAreaChanged().onNotify(this, [this]() {
        ensureViewportInsideScrollableArea();

        emit horizontalScrollChanged();
        emit verticalScrollChanged();
        emit viewportChanged();
    }, async::Asyncable::Mode::SetReplace);

    m_isAutomaticallyPanEnabled = notationConfiguration()->isAutomaticallyPanEnabled();
    notationConfiguration()->isAutomaticallyPanEnabledChanged().onNotify(this, [this]() {
        m_isAutomaticallyPanEnabled = notationConfiguration()->isAutomaticallyPanEnabled();
    }, async::Asyncable::Mode::SetReplace);

    m_isSmoothPanningEnabled = configuration()->isSmoothPanning();
    configuration()->isSmoothPanningChanged().onNotify(this, [this]() {
        m_isSmoothPanningEnabled = configuration()->isSmoothPanning();
    }, async::Asyncable::Mode::SetReplace);

    scheduleRedraw();
}

void AbstractNotationPaintView::initBackground()
{
    emit backgroundColorChanged(notationConfiguration()->backgroundColor());

    notationConfiguration()->backgroundChanged().onNotify(this, [this]() {
        emit backgroundColorChanged(notationConfiguration()->backgroundColor());
        scheduleRedraw();
    }, async::Asyncable::Mode::SetReplace);
}

void AbstractNotationPaintView::initNavigatorOrientation()
{
    notationConfiguration()->canvasOrientation().ch.onReceive(this, [this](muse::Orientation) {
        moveCanvasToPosition(PointF(0, 0));
    }, async::Asyncable::Mode::SetReplace);
}

void AbstractNotationPaintView::moveCanvasToCenter()
{
    TRACEFUNC;

    if (!isInited()) {
        return;
    }

    PointF canvasCenter = this->canvasCenter();
    Transform oldMatrix = m_matrix;

    if (doMoveCanvas(canvasCenter.x(), canvasCenter.y())) {
        onMatrixChanged(oldMatrix, m_matrix, false);
    }
}

void AbstractNotationPaintView::scrollHorizontal(qreal position)
{
    TRACEFUNC;

    qreal scrollStep = position - m_previousHorizontalScrollPosition;
    if (qFuzzyIsNull(scrollStep)) {
        return;
    }

    qreal dx = horizontalScrollableSize() * scrollStep;
    moveCanvasHorizontal(-dx);
}

void AbstractNotationPaintView::scrollVertical(qreal position)
{
    TRACEFUNC;

    qreal scrollStep = position - m_previousVerticalScrollPosition;
    if (qFuzzyIsNull(scrollStep)) {
        return;
    }

    qreal dy = verticalScrollableSize() * scrollStep;
    moveCanvasVertical(-dy);
}

void AbstractNotationPaintView::zoomIn()
{
    m_inputController->zoomIn();
}

void AbstractNotationPaintView::zoomOut()
{
    m_inputController->zoomOut();
}

void AbstractNotationPaintView::selectOnNavigationActive()
{
    TRACEFUNC;

    if (!notation()) {
        return;
    }

    auto interaction = notation()->interaction();
    if (!interaction->selection()->isNone()) {
        return;
    }

    interaction->selectFirstElement(false);
}

bool AbstractNotationPaintView::canReceiveAction(const ActionCode& actionCode) const
{
    if (actionCode == "diagnostic-notationview-redraw") {
        return true;
    }

    return hasFocus();
}

void AbstractNotationPaintView::onCurrentNotationChanged()
{
    TRACEFUNC;

    cancelRepriseSlur();
    m_repriseEscapeHeld = false;
    if (m_repriseToolModel) {
        m_repriseToolModel->reset();
    }

    if (m_notation) {
        onUnloadNotation(m_notation);
    }

    setNotation(globalContext()->currentNotation());

    if (!m_notation) {
        emit noteInputActiveChanged();
        emit repriseViewModeChanged();
        return;
    }

    onLoadNotation(m_notation);
    // Loading can restore the native view mode before its change subscription exists.
    emit repriseViewModeChanged();
}

void AbstractNotationPaintView::onLoadNotation(INotationPtr)
{
    if (viewport().isValid() && !m_notation->viewState()->isMatrixInited()) {
        initZoomAndPosition();
    }

    if (publishMode()) {
        m_notation->painting()->setViewMode(ViewMode::PAGE);
    } else {
        m_notation->painting()->setViewMode(m_notation->viewState()->viewMode());
    }

    m_notation->notationChanged().onNotify(this, [this]() {
        cancelRepriseSlur();
        updateLoopMarkers();
        updateShadowNoteVisibility();
        scheduleRedraw();
    });

    onNoteInputStateChanged();
    if (isNoteEnterMode()) {
        emit activeFocusRequested();
    }

    INotationInteractionPtr interaction = notationInteraction();

    interaction->noteInput()->stateChanged().onNotify(this, [this]() {
        onNoteInputStateChanged();
    });

    interaction->noteInput()->noteInputStarted().onReceive(this, [this](bool focusNotation) {
        if (focusNotation) {
            emit activeFocusRequested();
        }
    });

    interaction->selectionChanged().onNotify(this, [this]() {
        scheduleRedraw();
    });

    interaction->showItemRequested().onReceive(this, [this](const INotationInteraction::ShowItemRequest& request) {
        onShowItemRequested(request);
    });

    interaction->textEditingStarted().onNotify(this, [this]() {
        setFlag(ItemAcceptsInputMethod, true);
        setFocus(false); // Remove focus once so that the IME reloads the state
        forceFocusIn();
    });

    interaction->textEditingEnded().onReceive(this, [this](const engraving::TextBase*) {
        setFlag(ItemAcceptsInputMethod, false);
        setFocus(false); // Remove focus once so that the IME reloads the state
        forceFocusIn();
    });

    interaction->dropChanged().onNotify(this, [this]() {
        if (!hasActiveFocus()) {
            forceFocusIn(); // grab keyboard focus after element added from palette
        }
    });

    interaction->shadowNoteChanged().onReceive(this, [this](bool visible) {
        if (m_shadowNoteRect.isValid()) {
            scheduleRedraw(m_shadowNoteRect);

            if (!visible) {
                m_shadowNoteRect = RectF();
                return;
            }
        }

        RectF shadowNoteRect = fromLogical(notationInteraction()->shadowNoteRect());

        if (shadowNoteRect.isValid()) {
            compensateFloatPart(shadowNoteRect);
            scheduleRedraw(shadowNoteRect);
        }

        m_shadowNoteRect = shadowNoteRect;
    });

    updateLoopMarkers();
    // FIXME: only un-/re-subscribe when master notation changes
    notationPlayback()->loopBoundariesChanged().onNotify(this, [this]() {
        updateLoopMarkers();
    });

    m_notation->viewModeChanged().onNotify(this, [this]() {
        cancelRepriseSlur();
        ensureViewportInsideScrollableArea();
        emit repriseViewModeChanged();
    });

    if (isMainView()) {
        connect(this, &QQuickPaintedItem::focusChanged, this, [this](bool focused) {
            if (notation()) {
                notation()->accessibility()->setEnabled(focused);
            }
        });

        notation()->accessibility()->setMapToScreenFunc([this](const RectF& elementRect) {
            if (elementRect.isEmpty()) {
                return RectF(PointF::fromQPointF(mapToGlobal({ 0, 0 })), SizeF(width(), height()));
            }

            auto res = fromLogical(elementRect);
            res = RectF(PointF::fromQPointF(mapToGlobal(res.topLeft().toQPointF())), SizeF(res.width(), res.height()));

            return res;
        });

        notation()->interaction()->setGetViewRectFunc([this]() {
            return viewport();
        });
    }

    async::Async::call(this, [this](){
        forceFocusIn();
    });

    scheduleRedraw();

    emit horizontalScrollChanged();
    emit verticalScrollChanged();
    emit viewportChanged();
}

void AbstractNotationPaintView::onUnloadNotation(INotationPtr)
{
    m_notation->notationChanged().disconnect(this);
    INotationInteractionPtr interaction = m_notation->interaction();
    interaction->noteInput()->stateChanged().disconnect(this);
    interaction->noteInput()->noteInputStarted().disconnect(this);
    interaction->selectionChanged().disconnect(this);
    interaction->showItemRequested().disconnect(this);
    interaction->textEditingStarted().disconnect(this);
    interaction->textEditingEnded().disconnect(this);
    interaction->dropChanged().disconnect(this);
    interaction->shadowNoteChanged().disconnect(this);
    notationPlayback()->loopBoundariesChanged().disconnect(this);
    m_notation->viewModeChanged().disconnect(this);

    if (isMainView()) {
        disconnect(this, &QQuickPaintedItem::focusChanged, this, nullptr);
        m_notation->accessibility()->setMapToScreenFunc(nullptr);
        m_notation->interaction()->setGetViewRectFunc(nullptr);
    }
}

void AbstractNotationPaintView::initZoomAndPosition()
{
}

void AbstractNotationPaintView::setMatrix(const Transform& matrix)
{
    if (m_matrix == matrix) {
        return;
    }

    Transform oldMatrix = m_matrix;
    m_matrix = matrix;

    // If `ensureViewportInsideScrollableArea` returns true, it has already
    // notified about matrix changed, so no need to do it again.
    if (ensureViewportInsideScrollableArea()) {
        return;
    }

    onMatrixChanged(oldMatrix, m_matrix, false);
}

void AbstractNotationPaintView::onMatrixChanged(const Transform& oldMatrix, const Transform& newMatrix, bool overrideZoomType)
{
    cancelRepriseSlur();
    UNUSED(overrideZoomType);

    Transform oldMatrixInverted = oldMatrix.inverted();

    if (m_shadowNoteRect.isValid()) {
        RectF logicRect = oldMatrixInverted.map(m_shadowNoteRect);
        m_shadowNoteRect = newMatrix.map(logicRect);
    }

    scheduleRedraw();

    emit horizontalScrollChanged();
    emit verticalScrollChanged();
    emit matrixChanged();
    emit viewportChanged();

    onPlaybackCursorRectChanged();
}

void AbstractNotationPaintView::onViewSizeChanged()
{
    TRACEFUNC;

    const qreal newWidth = width();
    const qreal previousWidth = m_previousRepriseViewWidth;
    m_previousRepriseViewWidth = newWidth;

    if (!notation()) {
        return;
    }

    // At percentage zoom MuseScore keeps the old translation when the view
    // shrinks, leaving a formerly centered page stranded at the right edge.
    // Preserve the same score point at the viewport center instead. This
    // affects only Reprise's main view, not secondary scores or print views.
    if (m_isMainView && m_repriseToolModel && m_repriseToolModel->enabled()
        && previousWidth > 0 && newWidth > 0
        && !qFuzzyIsNull(newWidth - previousWidth)
        && notation()->viewState()->isMatrixInited()
        && notation()->viewState()->zoomType().val == ZoomType::Percentage) {
        const qreal scaling = currentScaling();
        if (!qFuzzyIsNull(scaling)) {
            Transform beforeResize = m_matrix;
            if (doMoveCanvas((newWidth - previousWidth) / (2.0 * scaling), 0.0)) {
                onMatrixChanged(beforeResize, m_matrix, false);
            }
        }
    }

    if (viewport().isValid()) {
        if (!notation()->viewState()->isMatrixInited()) {
            initZoomAndPosition();
        } else {
            m_inputController->updateZoomAfterSizeChange();
        }
    }

    ensureViewportInsideScrollableArea();

    scheduleRedraw();

    emit horizontalScrollChanged();
    emit verticalScrollChanged();
    emit viewportChanged();

    onPlaybackCursorRectChanged();
}

void AbstractNotationPaintView::updateLoopMarkers()
{
    TRACEFUNC;

    const LoopBoundaries& loop = notationPlayback()->loopBoundaries();

    m_loopInMarker->setVisible(loop.enabled);
    m_loopOutMarker->setVisible(loop.enabled);

    if (loop.enabled) {
        m_loopInMarker->updatePosition(loop.loopInTick);
        m_loopOutMarker->updatePosition(loop.loopOutTick);
    }

    scheduleRedraw();
}

void AbstractNotationPaintView::updateShadowNoteVisibility()
{
    INotationInteractionPtr interaction = notationInteraction();
    const engraving::ShadowNote* shadowNote = interaction ? interaction->shadowNote() : nullptr;
    if (!shadowNote || !shadowNote->visible()) {
        m_shadowNoteRect = RectF();
        return;
    }

    if (isNoteEnterMode() && !(usesRepriseEditing() && notationNoteInput()->usingNoteInputMethod(NoteInputMethod::REPITCH))) {
        //! NOTE: The following may actually hide the shadow note
        //! if cursorPos is no longer valid...
        const QPointF cursorPos = mapFromGlobal(QCursor::pos());
        interaction->showShadowNoteForPosition(toLogical(cursorPos));
    } else {
        interaction->hideShadowNote();
        m_shadowNoteRect = RectF();
        return;
    }
}

NotationViewInputController* AbstractNotationPaintView::inputController() const
{
    return m_inputController.get();
}

INotationPtr AbstractNotationPaintView::notation() const
{
    return m_notation;
}

INotationInteractionPtr AbstractNotationPaintView::notationInteraction() const
{
    return notation() ? notation()->interaction() : nullptr;
}

INotationPlaybackPtr AbstractNotationPaintView::notationPlayback() const
{
    return notation() ? notation()->masterNotation()->playback() : nullptr;
}

QQuickItem* AbstractNotationPaintView::asItem()
{
    return this;
}

INotationNoteInputPtr AbstractNotationPaintView::notationNoteInput() const
{
    return notationInteraction() ? notationInteraction()->noteInput() : nullptr;
}

INotationElementsPtr AbstractNotationPaintView::notationElements() const
{
    return notation() ? notation()->elements() : nullptr;
}

INotationStylePtr AbstractNotationPaintView::notationStyle() const
{
    return notation() ? notation()->style() : nullptr;
}

INotationSelectionPtr AbstractNotationPaintView::notationSelection() const
{
    return notationInteraction() ? notationInteraction()->selection() : nullptr;
}

bool AbstractNotationPaintView::speedyEntryActive() const
{
    return m_speedyEntrySelected && isNoteEnterMode()
           && usesRepriseEditing()
           && notationNoteInput()->usingNoteInputMethod(NoteInputMethod::BY_DURATION);
}

void AbstractNotationPaintView::setShowOnlyActiveVoice(bool enabled)
{
    if (m_showOnlyActiveVoice == enabled) {
        return;
    }

    m_showOnlyActiveVoice = enabled;
    emit showOnlyActiveVoiceChanged();
    scheduleRedraw();
}

void AbstractNotationPaintView::toggleSpeedyEntry()
{
    const auto input = notationNoteInput();
    if (!usesRepriseEditing() || !input) {
        return;
    }

    if (speedyEntryActive()) {
        dispatcher()->dispatch("note-input-by-duration");
        return;
    }

    m_speedyOctaveOffset = 0;
    if (input->isNoteInputMode()) {
        input->setNoteInputMethod(NoteInputMethod::BY_DURATION);
    } else {
        dispatcher()->dispatch("note-input-by-duration");
    }

    m_speedyEntrySelected = input->isNoteInputMode() && input->usingNoteInputMethod(NoteInputMethod::BY_DURATION);
    onNoteInputStateChanged();
}

void AbstractNotationPaintView::onNoteInputStateChanged()
{
    if (!isNoteEnterMode() || !notationNoteInput()->usingNoteInputMethod(NoteInputMethod::BY_DURATION)) {
        m_speedyEntrySelected = false;
    }
    if (m_repriseToolModel && isNoteEnterMode()) {
        m_repriseToolModel->reset();
        m_repriseEscapeHeld = false;
    }

    TRACEFUNC;

    emit noteInputActiveChanged();
    setAcceptHoverEvents(isNoteEnterMode() || m_playbackScrubHeld);
    updateLoopMarkers();
    updateShadowNoteVisibility();
    scheduleRedraw();
}

void AbstractNotationPaintView::onShowItemRequested(const INotationInteraction::ShowItemRequest& request)
{
    IF_ASSERT_FAILED(request.item) {
        return;
    }

    RectF viewRect = viewport();

    RectF showRect = request.showRect;
    RectF itemBoundingRect = request.item->canvasBoundingRect();

    if (viewRect.width() < showRect.width()) {
        showRect.setLeft(itemBoundingRect.x());
        showRect.setWidth(itemBoundingRect.width());
    }

    if (viewRect.height() < showRect.height()) {
        showRect.setTop(itemBoundingRect.y());
        showRect.setHeight(itemBoundingRect.height());
    }

    adjustCanvasPosition(showRect);
}

bool AbstractNotationPaintView::isNoteEnterMode() const
{
    return notationNoteInput() ? notationNoteInput()->isNoteInputMode() : false;
}

bool AbstractNotationPaintView::usesRepriseRangeCopy() const
{
    return usesRepriseEditing() && m_repriseToolModel->activeTool() == RepriseToolModel::Selection;
}

bool AbstractNotationPaintView::usesRepriseEditing() const
{
    return isMainView() && !publishMode() && !m_readonly && m_repriseToolModel && m_repriseToolModel->enabled();
}

void AbstractNotationPaintView::showContextMenu(const ElementType& elementType, const QPointF& pos)
{
    TRACEFUNC;

    QPointF _pos = pos;
    if (_pos.isNull()) {
        _pos = QPointF(width() / 2, height() / 2);
    }

    emit showContextMenuRequested(static_cast<int>(elementType), _pos);
}

void AbstractNotationPaintView::hideContextMenu()
{
    TRACEFUNC;

    if (m_isContextMenuOpen) {
        emit hideContextMenuRequested();
    }
}

void AbstractNotationPaintView::showElementPopup(const ElementType& elementType)
{
    TRACEFUNC;

    const PopupModelType modelType = AbstractElementPopupModel::modelTypeFromElement(elementType);
    if (m_currentElementPopupType == modelType) {
        // Don't do anything if a popup of this type is already open...
        return;
    }

    m_currentElementPopupType = modelType;
    emit showElementPopupRequested(modelType);
}

void AbstractNotationPaintView::hideElementPopup(const ElementType& elementType)
{
    TRACEFUNC;

    hideElementPopup(AbstractElementPopupModel::modelTypeFromElement(elementType));
}

void AbstractNotationPaintView::hideElementPopup(PopupModelType modelType)
{
    TRACEFUNC;

    if (m_currentElementPopupType == PopupModelType::TYPE_UNDEFINED) {
        // Popup is already hidden...
        return;
    }

    // Hide the popup if the model type matches the currently open popup type, or if no element type was specified
    if (modelType == m_currentElementPopupType || modelType == PopupModelType::TYPE_UNDEFINED) {
        m_currentElementPopupType = PopupModelType::TYPE_UNDEFINED;
        emit hideElementPopupRequested();
    }
}

void AbstractNotationPaintView::toggleElementPopup(const ElementType& elementType)
{
    if (m_currentElementPopupType != PopupModelType::TYPE_UNDEFINED) {
        hideElementPopup(elementType);
        return;
    }

    showElementPopup(elementType);
}

bool AbstractNotationPaintView::elementPopupIsOpen(const ElementType& elementType) const
{
    const PopupModelType modelType = AbstractElementPopupModel::modelTypeFromElement(elementType);
    return m_currentElementPopupType == modelType;
}

static void paintSpeedyEntryCursor(Painter* painter, const NoteInputState& input, const Color& color)
{
    if (!input.isValid()) {
        return;
    }

    const auto* segment = input.segment();
    const auto* staff = input.staff();
    const auto* system = segment ? segment->system() : nullptr;
    const auto* measure = segment ? segment->measure() : nullptr;
    if (!staff || !system || !measure) {
        return;
    }

    const double space = staff->spatium(input.tick());
    const double staffY = system->staffCanvasYpage(input.staffIdx());
    const double left = measure->canvasPos().x();
    const double top = staffY - 1.5 * space;
    const double width = measure->width();
    const double height = staff->staffHeight(input.tick()) + 3.0 * space;
    const double stroke = 0.10 * space;
    const double x = segment->canvasPos().x();

    painter->fillRect(RectF(left, top, width, stroke), color);
    painter->fillRect(RectF(left, top + height, width, stroke), color);
    painter->fillRect(RectF(left, top, stroke, height), color);
    painter->fillRect(RectF(left + width, top, stroke, height), color);
    painter->fillRect(RectF(x, top, stroke, height), color);

    if (!input.notes().empty()) {
        const int line = engraving::noteValToLine(input.notes().front(), staff, input.tick());
        const double y = staffY + line * staff->lineDistance(input.tick()) * space / 2;
        painter->fillRect(RectF(x - 0.65 * space, y, 1.4 * space, 1.5 * stroke), color);
    }
}

void AbstractNotationPaintView::paint(QPainter* qp)
{
    TRACEFUNC;

    RectF rect = RectF::fromQRectF(qp->clipBoundingRect());
    rect = correctDrawRect(rect);

    muse::draw::Painter mup(qp, objectName().toStdString());
    muse::draw::Painter* painter = &mup;

    paintBackground(rect, painter);

    if (!isInited()) {
        return;
    }

    qreal guiScaling = notationConfiguration()->guiScaling();
    Transform guiScalingCompensation;
    guiScalingCompensation.scale(guiScaling, guiScaling);

    painter->setWorldTransform(m_matrix * guiScalingCompensation);

    bool isPrinting = publishMode() || m_inputController->readonly();
    const int visibleVoice = !isPrinting && m_showOnlyActiveVoice && notationNoteInput()
                             ? static_cast<int>(notationNoteInput()->state().voice()) : -1;
    notation()->painting()->paintView(painter, toLogical(rect), isPrinting, visibleVoice);

    const ui::UiContext& uiCtx = uiContextResolver()->currentUiContext();
    const bool isOnNotationPage = uiCtx == ui::UiCtxProjectOpened || uiCtx == ui::UiCtxProjectFocused;

    const INotationNoteInputPtr noteInput = notationNoteInput();
    if (noteInput->isNoteInputMode() && isOnNotationPage) {
        if (speedyEntryActive()) {
            paintSpeedyEntryCursor(painter, noteInput->state(), engravingConfiguration()->selectionColor(noteInput->state().voice()));
        } else if (noteInput->usingNoteInputMethod(NoteInputMethod::BY_DURATION)
            && !notationConfiguration()->useNoteInputCursorInInputByDuration()) {
            m_ruler->paint(painter, noteInput->state());
        } else {
            m_noteInputCursor->paint(painter);
        }
    }

    m_loopInMarker->paint(painter);
    m_loopOutMarker->paint(painter);

    if (notation()->viewMode() == engraving::LayoutMode::LINE) {
        ContinuousPanel::NotationViewContext nvCtx;
        nvCtx.xOffset = m_matrix.dx();
        nvCtx.yOffset = m_matrix.dy();
        nvCtx.scaling = currentScaling();
        nvCtx.fromLogical = [this](const PointF& pos) -> PointF { return fromLogical(pos); };

        engraving::rendering::PaintOptions opt;
        opt.invertColors = notationConfiguration()->shouldInvertScore();
        m_continuousPanel->paint(*painter, nvCtx, opt);
    }

    paintRepriseSlurPreview(qp);
}

void AbstractNotationPaintView::onNotationSetup()
{
    TRACEFUNC;
    onCurrentNotationChanged();
    onPlayingChanged();

    globalContext()->currentNotationChanged().onNotify(this, [this]() {
        onCurrentNotationChanged();
    });

    playbackController()->isPlayingChanged().onNotify(this, [this]() {
        onPlayingChanged();
    });

    playbackController()->currentPlaybackPositionChanged().onReceive(this, [this](audio::secs_t, midi::tick_t tick) {
        movePlaybackCursor(tick);
    });

    notationConfiguration()->foregroundChanged().onNotify(this, [this]() {
        scheduleRedraw();
    });

    uiConfiguration()->currentThemeChanged().onNotify(this, [this]() {
        scheduleRedraw();
    }, async::Asyncable::Mode::SetReplace);

    engravingConfiguration()->selectionColorChanged().onReceive(this, [this](voice_idx_t, const muse::draw::Color&) {
        scheduleRedraw();
    });

    engravingConfiguration()->formattingColorChanged().onReceive(this, [this](const Color&) {
        scheduleRedraw();
    });

    engravingConfiguration()->invisibleColorChanged().onReceive(this, [this](const Color&) {
        scheduleRedraw();
    });

    engravingConfiguration()->unlinkedColorChanged().onReceive(this, [this](const Color&) {
        scheduleRedraw();
    });

    engravingConfiguration()->debuggingOptionsChanged().onNotify(this, [this]() {
        scheduleRedraw();
    });
}

void AbstractNotationPaintView::paintBackground(const RectF& rect, muse::draw::Painter* painter)
{
    TRACEFUNC;

    const QPixmap& wallpaper = notationConfiguration()->backgroundWallpaper();

    if (notationConfiguration()->backgroundUseColor() || wallpaper.isNull()) {
        painter->fillRect(rect, notationConfiguration()->backgroundColor());
    } else {
        painter->drawTiledPixmap(rect, wallpaper, rect.topLeft() - PointF(m_matrix.m31(), m_matrix.m32()));
    }
}

PointF AbstractNotationPaintView::canvasCenter() const
{
    TRACEFUNC;
    RectF canvasRect = m_matrix.map(notationContentRect());

    qreal canvasWidth = canvasRect.width();
    qreal canvasHeight = canvasRect.height();

    qreal x = (width() - canvasWidth) / 2;
    qreal y = (height() - canvasHeight) / 2;

    return toLogical(PointF(x, y));
}

std::pair<qreal, qreal> AbstractNotationPaintView::constraintCanvas(qreal dx, qreal dy) const
{
    TRACEFUNC;
    RectF scrollableArea = scrollableAreaRect();
    RectF viewport = this->viewport();

    // horizontal
    {
        qreal newLeft = viewport.left() - dx;
        if (viewport.width() > scrollableArea.width()) {
            newLeft = scrollableArea.center().x() - viewport.width() / 2;
        } else {
            newLeft = qBound(scrollableArea.left(), newLeft, scrollableArea.right() - viewport.width());
        }
        dx = viewport.left() - newLeft;
    }

    // vertical
    {
        qreal newTop = viewport.top() - dy;
        if (viewport.height() > scrollableArea.height()) {
            newTop = scrollableArea.center().y() - viewport.height() / 2;
        } else {
            newTop = qBound(scrollableArea.top(), newTop, scrollableArea.bottom() - viewport.height());
        }
        dy = viewport.top() - newTop;
    }

    return { dx, dy };
}

QVariant AbstractNotationPaintView::matrix() const
{
    return Transform::toQTransform(m_matrix);
}

PointF AbstractNotationPaintView::viewportTopLeft() const
{
    return toLogical(PointF(0.0, 0.0));
}

RectF AbstractNotationPaintView::viewport() const
{
    return toLogical(RectF(0.0, 0.0, width(), height()));
}

QRectF AbstractNotationPaintView::viewport_property() const
{
    return viewport().toQRectF();
}

RectF AbstractNotationPaintView::notationContentRect() const
{
    TRACEFUNC;
    if (!notationElements()) {
        return RectF();
    }

    RectF result;
    for (const Page* page: notationElements()->pages()) {
        result.unite(page->ldata()->bbox().translated(page->pos()));
    }

    return result;
}

RectF AbstractNotationPaintView::scrollableAreaRect() const
{
    TRACEFUNC;
    RectF viewport = this->viewport();
    qreal overscrollFactor = configuration()->isLimitCanvasScrollArea() ? 0.0 : SCROLL_LIMIT_OFF_OVERSCROLL_FACTOR;

    qreal overscrollX = viewport.width() * overscrollFactor;
    qreal overscrollY = viewport.height() * overscrollFactor;

    return notationContentRect().adjusted(-overscrollX, -overscrollY, overscrollX, overscrollY);
}

qreal AbstractNotationPaintView::horizontalScrollableSize() const
{
    TRACEFUNC;
    RectF viewport = this->viewport();
    RectF contentRect = notationContentRect();

    qreal left = std::min(contentRect.left(), viewport.left());
    qreal right = std::max(contentRect.right(), viewport.right());

    qreal size = 0;
    if ((left < 0) && (right > 0)) {
        size = std::abs(left) + right;
    } else {
        size = std::abs(right) - std::abs(left);
    }

    return size;
}

qreal AbstractNotationPaintView::verticalScrollableSize() const
{
    TRACEFUNC;
    RectF viewport = this->viewport();
    RectF contentRect = notationContentRect();

    qreal top = std::min(contentRect.top(), viewport.top());
    qreal bottom = std::max(contentRect.bottom(), viewport.bottom());

    qreal size = 0;
    if ((top < 0) && (bottom > 0)) {
        size = std::abs(top) + bottom;
    } else {
        size = std::abs(bottom) - std::abs(top);
    }

    return size;
}

qreal AbstractNotationPaintView::horizontalScrollbarSize() const
{
    TRACEFUNC;
    RectF viewport = this->viewport();
    RectF contentRect = notationContentRect();

    if (viewport.left() < contentRect.left()
        && viewport.right() > contentRect.right()) {
        return 0;
    }

    qreal scrollableWidth = horizontalScrollableSize();
    if (qFuzzyIsNull(scrollableWidth)) {
        return 0;
    }

    return viewport.width() / scrollableWidth;
}

qreal AbstractNotationPaintView::verticalScrollbarSize() const
{
    TRACEFUNC;
    RectF viewport = this->viewport();
    RectF contentRect = notationContentRect();

    if (viewport.top() < contentRect.top()
        && viewport.bottom() > contentRect.bottom()) {
        return 0;
    }

    qreal scrollableHeight = verticalScrollableSize();
    if (qFuzzyIsNull(scrollableHeight)) {
        return 0;
    }

    return viewport.height() / scrollableHeight;
}

qreal AbstractNotationPaintView::startHorizontalScrollPosition() const
{
    TRACEFUNC;
    RectF viewport = this->viewport();
    RectF contentRect = notationContentRect();

    if (!viewport.isValid() || !contentRect.isValid()) {
        return 0.0;
    }

    return (viewport.left() - contentRect.left()) / contentRect.width();
}

qreal AbstractNotationPaintView::startVerticalScrollPosition() const
{
    TRACEFUNC;
    RectF viewport = this->viewport();
    RectF contentRect = notationContentRect();

    if (!viewport.isValid() || !contentRect.isValid()) {
        return 0.0;
    }

    return (viewport.top() - contentRect.top()) / contentRect.height();
}

bool AbstractNotationPaintView::adjustCanvasPosition(const RectF& logicRect, bool adjustVertically)
{
    TRACEFUNC;

    RectF viewRect = viewport();
    PointF pos = viewRect.topLeft();

    // Account for continuous panel if panning in continuous view
    qreal continuousPanelWidth = 0;
    if (notation()->viewMode() == engraving::LayoutMode::LINE) {
        continuousPanelWidth = m_continuousPanel->width();
    }
    viewRect.adjust(continuousPanelWidth, 0, 0, 0);

    double viewArea = viewRect.width() * viewRect.height();
    double logicRectArea = logicRect.width() * logicRect.height();

    if (viewArea < logicRectArea) {
        return false;
    }

    if (viewRect.contains(logicRect)) {
        return false;
    }

    constexpr int BORDER_SPACING_RATIO = 3;

    double _spatium = notationStyle()->styleValue(StyleId::spatium).toDouble();
    qreal border = _spatium * BORDER_SPACING_RATIO;
    qreal _scale = currentScaling();
    if (qFuzzyIsNull(_scale)) {
        _scale = 1;
    }

    PointF oldPos = pos;

    RectF showRect = logicRect;

    if (showRect.left() < viewRect.left()) {
        pos.setX(showRect.left() - border - continuousPanelWidth);
    } else if (showRect.left() > viewRect.right()) {
        pos.setX(showRect.right() - width() / _scale + border);
    } else if (viewRect.width() >= showRect.width() && showRect.right() > viewRect.right()) {
        pos.setX(showRect.left() - border - continuousPanelWidth);
    }

    if (adjustVertically) {
        if (showRect.top() < viewRect.top() && showRect.bottom() < viewRect.bottom()) {
            pos.setY(showRect.top() - border);
        } else if (showRect.top() > viewRect.bottom()) {
            pos.setY(showRect.bottom() - height() / _scale + border);
        } else if (viewRect.height() >= showRect.height() && showRect.bottom() > viewRect.bottom()) {
            pos.setY(showRect.top() - border);
        }
    }

    pos = alignToCurrentPageBorder(showRect, pos);

    if (pos == oldPos) {
        return false;
    }

    return moveCanvasToPosition(pos);
}

bool AbstractNotationPaintView::adjustCanvasPositionSmoothPan(const RectF& cursorRect)
{
    RectF viewRect = viewport();
    PointF pos(cursorRect.x() - (viewRect.width() / 2), viewRect.y());

    if (!viewport().intersects(cursorRect)) {
        pos.setY(cursorRect.y() - (viewRect.height() / 2));
    }

    return moveCanvasToPosition(pos);
}

bool AbstractNotationPaintView::ensureViewportInsideScrollableArea()
{
    TRACEFUNC;

    if (!m_notation) {
        return false;
    }

    // When the viewport is empty, which might happen for example when the view
    // is being loaded or unloaded, the scrollable area is computed without any
    // overscroll allowance, resulting in unexpected clamping of the canvas
    // position to (0, 0).
    if (size().isEmpty()) {
        return false;
    }

    auto [dx, dy] = constraintCanvas(0, 0);
    if (qFuzzyIsNull(dx) && qFuzzyIsNull(dy)) {
        return false;
    }

    Transform oldMatrix = m_matrix;
    m_matrix.translate(dx, dy);
    onMatrixChanged(oldMatrix, m_matrix, false);
    return true;
}

bool AbstractNotationPaintView::moveCanvasToPosition(const PointF& logicPos)
{
    TRACEFUNC;

    PointF viewTopLeft = viewportTopLeft();
    Transform oldMatrix = m_matrix;
    if (doMoveCanvas(viewTopLeft.x() - logicPos.x(), viewTopLeft.y() - logicPos.y())) {
        onMatrixChanged(oldMatrix, m_matrix, false);
        return true;
    }

    return false;
}

bool AbstractNotationPaintView::moveCanvas(qreal dx, qreal dy)
{
    TRACEFUNC;

    Transform oldMatrix = m_matrix;
    bool moved = doMoveCanvas(dx, dy);

    if (moved) {
        onMatrixChanged(oldMatrix, m_matrix, false);

        m_autoScrollEnabled = false;
        m_enableAutoScrollTimer.start(2000);

        hideElementPopup();
    }

    return moved;
}

bool AbstractNotationPaintView::doMoveCanvas(qreal dx, qreal dy)
{
    if (!m_notation) {
        return false;
    }

    if (qFuzzyIsNull(dx) && qFuzzyIsNull(dy)) {
        return false;
    }

    auto [correctedDX, correctedDY] = constraintCanvas(dx, dy);
    if (qFuzzyIsNull(correctedDX) && qFuzzyIsNull(correctedDY)) {
        return false;
    }

    m_matrix.translate(correctedDX, correctedDY);

    return true;
}

void AbstractNotationPaintView::scheduleRedraw(const muse::RectF& rect)
{
    QRect qrect = correctDrawRect(rect).toQRect();
    update(qrect);
}

RectF AbstractNotationPaintView::correctDrawRect(const RectF& rect) const
{
    if (!rect.isValid() || rect.isNull()) {
        return RectF(0, 0, width(), height());
    }

    return rect;
}

void AbstractNotationPaintView::moveCanvasVertical(qreal dy)
{
    moveCanvas(0, dy);
}

void AbstractNotationPaintView::moveCanvasHorizontal(qreal dx)
{
    moveCanvas(dx, 0);
}

qreal AbstractNotationPaintView::currentScaling() const
{
    return m_matrix.m11();
}

void AbstractNotationPaintView::setScaling(qreal scaling, const PointF& pos, bool overrideZoomType)
{
    TRACEFUNC;

    if (!m_notation) {
        return;
    }

    hideElementPopup();

    qreal currentScaling = this->currentScaling();

    IF_ASSERT_FAILED(!qFuzzyIsNull(scaling)) {
        return;
    }

    if (qFuzzyCompare(currentScaling, scaling)) {
        return;
    }

    if (qFuzzyIsNull(currentScaling)) {
        currentScaling = 1;
    }

    qreal deltaScaling = scaling / currentScaling;
    scale(deltaScaling, pos, overrideZoomType);
}

void AbstractNotationPaintView::scale(qreal factor, const PointF& pos, bool overrideZoomType)
{
    TRACEFUNC;

    if (!m_notation) {
        return;
    }

    if (qFuzzyCompare(factor, 1.0)) {
        return;
    }

    PointF pointBeforeScaling = toLogical(pos);

    Transform oldMatrix = m_matrix;
    m_matrix.scale(factor, factor);

    PointF pointAfterScaling = toLogical(pos);

    qreal dx = pointAfterScaling.x() - pointBeforeScaling.x();
    qreal dy = pointAfterScaling.y() - pointBeforeScaling.y();

    doMoveCanvas(dx, dy);

    onMatrixChanged(oldMatrix, m_matrix, overrideZoomType);
}

void AbstractNotationPaintView::pinchToZoom(qreal scaleFactor, const QPointF& pos)
{
    if (isInited()) {
        m_inputController->pinchToZoom(scaleFactor, pos);
    }
}

void AbstractNotationPaintView::wheelEvent(QWheelEvent* event)
{
    TRACEFUNC;
    cancelRepriseSlur();
    if (isInited()) {
        m_inputController->wheelEvent(event);
    }
}

void AbstractNotationPaintView::forceFocusIn()
{
    TRACEFUNC;
    setFocus(true);
    emit activeFocusRequested();
    forceActiveFocus();
}

void AbstractNotationPaintView::onContextMenuIsOpenChanged(bool open)
{
    m_isContextMenuOpen = open;
}

void AbstractNotationPaintView::onElementPopupIsOpenChanged(const PopupModelType& popupType)
{
    m_currentElementPopupType = popupType;
}

void AbstractNotationPaintView::mousePressEvent(QMouseEvent* event)
{
    TRACEFUNC;
    forceFocusIn();

    if (speedyEntryActive() && event->button() == Qt::LeftButton && event->modifiers() == Qt::NoModifier) {
        const auto input = notationNoteInput();
        const auto* previousStaff = input->state().staff();
        if (previousStaff) {
            auto* score = previousStaff->score();
            engraving::staff_idx_t staffIndex = muse::nidx;
            engraving::Segment* segment = nullptr;
            if (score->pos2measure(toLogical(PointF::fromQPointF(event->position())), &staffIndex, nullptr, &segment, nullptr)) {
                const auto track = staffIndex * engraving::VOICES + input->state().voice();
                auto* entry = segment->element(track);
                if (entry && entry->isChord()) {
                    entry = engraving::toChord(entry)->upNote();
                }
                if (entry) {
                    notationInteraction()->select({ entry }, SelectType::SINGLE, staffIndex);
                }
                score->inputState().setSegment(segment);
                input->setCurrentTrack(track);
            }
        }
        event->accept();
        return;
    }

    if (m_reprisePointer.ownsPointer()) {
        cancelRepriseSlur();
        event->accept();
        return;
    }
    if (beginRepriseSlur(event, false)) {
        event->accept();
        return;
    }
    if (isInited()) {
        m_inputController->mousePressEvent(event);
    }
}

void AbstractNotationPaintView::mouseMoveEvent(QMouseEvent* event)
{
    TRACEFUNC;
    if (m_reprisePointer.ownsPointer()) {
        if (!(event->buttons() & Qt::LeftButton)) {
            cancelRepriseSlur();
            m_reprisePointer.reset();
        } else if (m_repriseToolModel && m_repriseToolModel->operationActive()) {
            if (event->modifiers() != Qt::NoModifier) {
                cancelRepriseSlur();
            } else {
                updateRepriseSlurPreview(event->position());
            }
        }
        event->accept();
        return;
    }
    if (isInited()) {
        m_inputController->mouseMoveEvent(event);
    }
}

void AbstractNotationPaintView::mouseDoubleClickEvent(QMouseEvent* event)
{
    TRACEFUNC;
    forceFocusIn();

    // Qt Quick receives the second press before MouseButtonDblClick.
    // Promote that passive owned press; never revive a cancelled gesture.
    if (m_reprisePointer.ownsPointer()
        && (!m_reprisePointer.pending() || !m_repriseToolModel || m_repriseToolModel->operationActive())) {
        cancelRepriseSlur();
        event->accept();
        return;
    }
    if (beginRepriseSlur(event, true)) {
        event->accept();
        return;
    }
    if (m_reprisePointer.ownsPointer()) {
        cancelRepriseSlur();
        event->accept();
        return;
    }
    if (isInited()) {
        m_inputController->mouseDoubleClickEvent(event);
    }
}

void AbstractNotationPaintView::mouseReleaseEvent(QMouseEvent* event)
{
    if (speedyEntryActive() && event->button() == Qt::LeftButton && event->modifiers() == Qt::NoModifier) {
        event->accept();
        return;
    }
    if (m_reprisePointer.ownsPointer()) {
        if (event->button() == Qt::LeftButton) {
            auto commit = std::move(m_repriseSlurCommit);
            const auto release = m_reprisePointer.release();
            if (m_repriseToolModel) {
                m_repriseToolModel->finishOperation();
            }
            if (release.commitCandidate && commit && event->modifiers() == Qt::NoModifier) {
                commit(event->position());
            }
        } else {
            cancelRepriseSlur();
        }
        event->accept();
        return;
    }
    if (isInited()) {
        m_inputController->mouseReleaseEvent(event);
    }
}

void AbstractNotationPaintView::mouseUngrabEvent()
{
    cancelRepriseSlur();
    m_reprisePointer.reset();
    muse::uicomponents::QuickPaintedView::mouseUngrabEvent();
}

void AbstractNotationPaintView::hoverMoveEvent(QHoverEvent* event)
{
    if (m_playbackScrubHeld && canScrubPlayback()) {
        scrubPlaybackAt(event->position());
        event->accept();
        return;
    }
    if (isInited()) {
        m_inputController->hoverMoveEvent(event);
    }
}

void AbstractNotationPaintView::hoverLeaveEvent(QHoverEvent* event)
{
    clearPlaybackScrubNotes();
    if (isInited()) {
        m_inputController->hoverLeaveEvent(event);
    }
}

static int speedyKeyboardPitch(const QKeyEvent* event)
{
#ifdef Q_OS_WIN
    if (!(GetKeyState(VK_CAPITAL) & 1) || event->modifiers() != Qt::NoModifier) {
        return -1;
    }

    const QString keys = QStringLiteral("ZXCVBNMASDFGHJQWERTYU");
    const int index = event->key() >= Qt::Key_A && event->key() <= Qt::Key_Z ? keys.indexOf(QChar(event->key())) : -1;
    if (index >= 0) {
        static constexpr int offsets[] = { 0, 2, 4, 5, 7, 9, 11 };
        return 48 + 12 * (index / 7) + offsets[index % 7];
    }

    switch (event->key()) {
    case Qt::Key_I: return -2;
    case Qt::Key_K: return -3;
    case Qt::Key_Comma: return -4;
    default: break;
    }
#else
    (void)event;
#endif
    return -1;
}

static bool isSpeedyEntryDirectCommand(const QKeyEvent* event)
{
    const auto modifiers = event->modifiers() & ~Qt::KeypadModifier;
    return speedyKeyboardPitch(event) != -1
           || (modifiers == Qt::NoModifier && event->key() == Qt::Key_Backspace)
           || (modifiers == Qt::ShiftModifier && (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down))
           || ((modifiers == Qt::NoModifier || modifiers == Qt::ShiftModifier)
               && (event->key() == Qt::Key_Apostrophe || event->key() == Qt::Key_QuoteDbl))
           || (event->modifiers() == Qt::NoModifier && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter))
           || (event->modifiers() == Qt::KeypadModifier && event->key() == Qt::Key_Enter);
}

static void deleteSpeedyPitch(const INotationPtr& notation, const NoteInputState& state)
{
    auto* entry = state.cr();
    if (!entry || !entry->isChord() || state.notes().empty()) {
        return;
    }

    auto* chord = engraving::toChord(entry);
    auto* note = chord->downNote();
    if (chord->notes().size() > 1) {
        note = nullptr;
        const int targetLine = engraving::noteValToLine(state.notes().front(), state.staff(), state.tick());
        for (auto* candidate : chord->notes()) {
            if (engraving::noteValToLine(candidate->noteVal(), state.staff(), state.tick()) == targetLine) {
                note = candidate;
                break;
            }
        }
    }
    if (!note) {
        return;
    }

    const auto undoStack = notation->undoStack();
    undoStack->prepareChanges(TranslatableString("undoableAction", "Delete note"));
    note->score()->deleteItem(note);
    undoStack->commitChanges();
}

static bool changeSpeedyChordDuration(const INotationPtr& notation, engraving::ChordRest* entry,
                                      const char* action)
{
    if (!entry || !entry->isChord()) {
        return false;
    }

    const auto type = NotationUiActions::actionDurationType(action);
    if (type == engraving::DurationType::V_INVALID) {
        return false;
    }

    engraving::TDuration duration(type);
    duration.setDots(entry->durationType().dots());
    if (duration == entry->durationType()) {
        return true;
    }

    const auto targetEnd = entry->tick() + duration.fraction() * entry->actualTicks() / entry->ticks();
    if (targetEnd > entry->measure()->tick() + entry->measure()->ticks()) {
        return true;
    }
    for (const auto* segment = entry->segment()->next(); segment && segment->tick() < targetEnd; segment = segment->next()) {
        const auto* following = segment->element(entry->track());
        if (following && !following->isRest() && following->isChordRest()) {
            return true;
        }
    }

    const auto undoStack = notation->undoStack();
    undoStack->prepareChanges(TranslatableString("undoableAction", "Change note duration"));
    entry->score()->changeCRlen(entry, duration);
    undoStack->commitChanges();
    return true;
}

static const char* speedyEntryAction(const QKeyEvent* event)
{
    const auto modifiers = event->modifiers() & ~Qt::KeypadModifier;
    if ((modifiers == Qt::NoModifier || modifiers == (Qt::ControlModifier | Qt::ShiftModifier))
        && event->key() >= Qt::Key_1 && event->key() <= Qt::Key_7) {
        static constexpr const char* durations[] = {
            "pad-note-64", "pad-note-32", "pad-note-16", "pad-note-8", "pad-note-4", "pad-note-2", "pad-note-1"
        };
        return durations[event->key() - Qt::Key_1];
    }
    if (modifiers == Qt::ControlModifier) {
        static constexpr const char* tuplets[] = {
            "tuplet-dialog", "duplet", "triplet", "quadruplet", "quintuplet", "sextuplet", "septuplet", "octuplet"
        };
        if (event->key() >= Qt::Key_1 && event->key() <= Qt::Key_8) {
            return tuplets[event->key() - Qt::Key_1];
        }
        return event->key() == Qt::Key_0 ? "pad-note-128" : nullptr;
    }
    if (modifiers == Qt::ShiftModifier) {
        switch (event->key()) {
        case Qt::Key_B: return "beam-auto";
        case Qt::Key_Left: return "notation-move-left-quickly";
        case Qt::Key_Right: return "notation-move-right-quickly";
        default: return nullptr;
        }
    }
    if (modifiers != Qt::NoModifier) {
        return nullptr;
    }

    switch (event->key()) {
    case Qt::Key_8: return "note-breve";
    case Qt::Key_BracketLeft: return "notation-move-left-quickly";
    case Qt::Key_BracketRight: return "notation-move-right-quickly";
    case Qt::Key_Period: return "pad-dot";
    case Qt::Key_X: return "sharp2-post";
    case Qt::Key_S: return "sharp-post";
    case Qt::Key_N: return "nat-post";
    case Qt::Key_F: return "flat-post";
    case Qt::Key_V: return "flat2-post";
    case Qt::Key_R: return "rest";
    case Qt::Key_Delete: return "action://notation/delete";
    case Qt::Key_O:
    case Qt::Key_H: return "toggle-visible";
    case Qt::Key_L: return "flip";
    case Qt::Key_Equal:
    case Qt::Key_T: return "tie";
    default: return nullptr;
    }
}

bool AbstractNotationPaintView::shortcutOverride(QKeyEvent* event)
{
    if (isInited()) {
        if (m_inputController->shortcutOverrideEvent(event)) {
            return true;
        }
        return (speedyEntryActive() && hasActiveFocus() && !notationInteraction()->isTextEditingStarted() && (isSpeedyEntryDirectCommand(event) || speedyEntryAction(event)))
               || isPlaybackScrubShortcut(event) || repriseAccidentalStepNote(event)
               || isRepriseKeypadTie(event)
               || (canRouteRepriseEscape(event) && (m_repriseEscapeHeld || m_repriseToolModel->canCancel()));
    }

    return false;
}

static mu::engraving::Note* repriseNormalNote(mu::engraving::EngravingItem* item)
{
    if (!item || !item->isNote()) {
        return nullptr;
    }
    auto note = mu::engraving::toNote(item);
    if (!note->chord() || note->isGrace() || note->isTrillCueNote() || note->chord()->isTrillCueNote()) {
        return nullptr;
    }
    return note;
}

static mu::engraving::Note* repriseNearestNormalNote(const INotationPtr& notation, const PointF& position,
                                                     mu::engraving::staff_idx_t preferredStaff = muse::nidx,
                                                     mu::engraving::track_idx_t requiredTrack = muse::nidx,
                                                     double maxDistance = std::numeric_limits<double>::max(),
                                                     const mu::engraving::Note* excludedNote = nullptr)
{
    if (!notation || !notation->elements() || !notation->interaction()) {
        return nullptr;
    }

    const bool distanceLimited = maxDistance != std::numeric_limits<double>::max();
    mu::engraving::staff_idx_t staffIdx = preferredStaff;
    bool searchAllStaves = false;
    if (staffIdx == muse::nidx) {
        const mu::engraving::Staff* staff = notation->interaction()->hitStaff(position);
        if (staff) {
            staffIdx = staff->idx();
        } else if (distanceLimited) {
            // For Reprise gestures, allow a modest capture halo just outside the
            // strict staff hit region without ever searching the whole score at
            // unlimited distance.
            searchAllStaves = true;
        } else {
            return nullptr;
        }
    }

    const double maxDistanceSquared = distanceLimited ? maxDistance * maxDistance : std::numeric_limits<double>::max();
    mu::engraving::Note* best = nullptr;
    double bestDistanceSquared = std::numeric_limits<double>::max();

    auto considerCandidate = [&](EngravingItem* item) {
        mu::engraving::Note* note = repriseNormalNote(item);
        if (!note
            || (!searchAllStaves && note->staffIdx() != staffIdx)
            || (requiredTrack != muse::nidx && note->track() != requiredTrack)
            || (excludedNote && (note == excludedNote || note->tick() == excludedNote->tick()))) {
            return;
        }

        const PointF center = note->canvasBoundingRect().center();
        const double dx = center.x() - position.x();
        const double dy = center.y() - position.y();
        const double distanceSquared = dx * dx + dy * dy;
        if (distanceLimited && distanceSquared > maxDistanceSquared) {
            return;
        }
        if (distanceSquared < bestDistanceSquared) {
            bestDistanceSquared = distanceSquared;
            best = note;
        }
    };

    // For an interactive drag, use MuseScore's complete hit area first.
    // This avoids losing a notehead because another engraving item is topmost.
    if (distanceLimited) {
        for (EngravingItem* item : notation->interaction()->hitElements(position, static_cast<float>(maxDistance))) {
            considerCandidate(item);
        }
        if (best) {
            return best;
        }
    }

    // Geometric fallback, also used for Finale-like slur release away from a note.
    FilterElementsOptions options;
    options.elementType = ElementType::NOTE;
    if (!searchAllStaves) {
        options.staffStart = static_cast<int>(staffIdx);
        options.staffEnd = static_cast<int>(staffIdx + 1);
    }
    for (EngravingItem* item : notation->elements()->elements(options)) {
        considerCandidate(item);
    }

    return best;
}

static bool repriseIsLastNormalNoteOnStaff(const INotationPtr& notation, const mu::engraving::Note* note)
{
    if (!notation || !notation->elements() || !note) {
        return false;
    }
    FilterElementsOptions options;
    options.elementType = ElementType::NOTE;
    options.staffStart = static_cast<int>(note->staffIdx());
    options.staffEnd = static_cast<int>(note->staffIdx() + 1);
    for (EngravingItem* item : notation->elements()->elements(options)) {
        mu::engraving::Note* candidate = repriseNormalNote(item);
        if (candidate && candidate->tick() > note->tick()) {
            return false;
        }
    }
    return true;
}

static std::optional<Fraction> repriseTimeAnchorTick(const INotationPtr& notation, const PointF& position,
                                                     mu::engraving::staff_idx_t staffIdx)
{
    if (!notation || !notation->elements()) {
        return std::nullopt;
    }
    mu::engraving::Score* score = notation->elements()->msScore();
    if (!score) {
        return std::nullopt;
    }

    mu::engraving::staff_idx_t hitStaff = staffIdx;
    mu::engraving::Segment* segment = nullptr;
    mu::engraving::Measure* measure = score->pos2measure(position, &hitStaff, nullptr, &segment, nullptr);
    if (!measure || hitStaff != staffIdx) {
        return std::nullopt;
    }

    // Reuse MuseScore's own time-anchor grid. It is the same mechanism used by
    // time-anchored lines and remains independent of note/rest presence.
    mu::engraving::EditTimeTickAnchors::updateAnchors(measure, staffIdx);
    segment = nullptr;
    hitStaff = staffIdx;
    score->dragPosition(position, &hitStaff, &segment, 0.5, true);
    score->hideAnchors();
    if (!segment || hitStaff != staffIdx) {
        return std::nullopt;
    }
    return segment->tick();
}

static bool repriseAddHairpinAtTicks(const INotationPtr& notation, const Fraction& startTick, const Fraction& endTick,
                                     mu::engraving::track_idx_t track)
{
    if (!notation || !notation->elements() || endTick <= startTick) {
        return false;
    }
    auto undoStack = notation->undoStack();
    mu::engraving::Score* score = notation->elements()->msScore();
    if (!undoStack || !score || undoStack->isLocked()) {
        return false;
    }

    undoStack->prepareChanges(muse::TranslatableString("undoableAction", "Add hairpin"));
    mu::engraving::Hairpin* hairpin = score->addHairpin(mu::engraving::HairpinType::CRESC_HAIRPIN, startTick, endTick, track);
    if (!hairpin) {
        undoStack->rollbackChanges();
        return false;
    }
    hairpin->setInitialTrackAndVoiceAssignment(hairpin->track(), false);
    undoStack->commitChanges();
    return true;
}

static bool repriseAddFreeEndSlur(const INotationPtr& notation, mu::engraving::Note* startNote, const PointF& endOffset)
{
    if (!notation || !notation->elements() || !startNote || !startNote->chord()) {
        return false;
    }
    auto undoStack = notation->undoStack();
    mu::engraving::Score* score = notation->elements()->msScore();
    if (!undoStack || !score || undoStack->isLocked()) {
        return false;
    }

    undoStack->prepareChanges(muse::TranslatableString("undoableAction", "Add slur"));
    mu::engraving::Slur* slur = score->addSlur(startNote->chord(), startNote->chord(), nullptr);
    if (!slur || !slur->frontSegment()) {
        undoStack->rollbackChanges();
        return false;
    }
    slur->frontSegment()->setSlurOffset(mu::engraving::Grip::END, endOffset);
    undoStack->commitChanges();
    return true;
}

bool AbstractNotationPaintView::repriseSlurInputAvailable() const
{
    return isInited() && m_notation && hasActiveFocus() && isMainView() && !publishMode() && !m_readonly
           && m_repriseToolModel && m_repriseToolModel->enabled()
           && m_repriseToolModel->activeTool() == RepriseToolModel::SmartShape
           && !m_isContextMenuOpen && m_currentElementPopupType == PopupModelType::TYPE_UNDEFINED
           && !m_inputController->hasNativeEscapePriority(true);
}

void AbstractNotationPaintView::cancelRepriseSlur()
{
    m_repriseSlurCommit = {};
    m_reprisePointer.cancel();
    clearRepriseSlurPreview();
    if (m_repriseToolModel && m_repriseToolModel->operationActive()) {
        m_repriseToolModel->finishOperation();
    }
}

bool AbstractNotationPaintView::beginRepriseSlur(QMouseEvent* event, bool startGesture)
{
    if (event->button() != Qt::LeftButton || event->buttons() != Qt::LeftButton
        || event->modifiers() != Qt::NoModifier || !repriseSlurInputAvailable()) {
        return false;
    }
    auto note = repriseNormalNote(m_inputController->hitElementAtViewPosition(event->position()));
    if (!note) {
        const qreal scaling = std::max(currentScaling(), 0.001);
        note = repriseNearestNormalNote(m_notation, toLogical(event->position()), muse::nidx, muse::nidx,
                                        REPRISE_NOTE_START_RADIUS_PX / scaling);
        if (!note) {
            // Inside a staff, preserve Finale-like nearest-note selection even
            // when notes are spaced farther apart than the capture halo.
            note = repriseNearestNormalNote(m_notation, toLogical(event->position()));
        }
    }
    const bool promotePassivePress = startGesture && m_reprisePointer.ownsPointer()
                                     && m_reprisePointer.pending() && !m_repriseToolModel->operationActive();
    if (!note || (!promotePassivePress && !m_reprisePointer.begin())) {
        return false;
    }
    // The first press owns its release but cannot create music or a preview.
    // The following double-click event promotes the second held press.
    if (!startGesture) {
        return true;
    }
    if (!m_repriseToolModel->beginOperation()) {
        m_reprisePointer.reset();
        return false;
    }
    LOGI() << "Reprise Smart Shape double-click gesture started, shape=" << m_repriseToolModel->activeShape();
    m_repriseSlurPressPosition = event->position();
    m_repriseSlurStartRect = fromLogical(note->canvasBoundingRect()).toQRectF();
    m_repriseSlurPreviewStart = m_repriseSlurStartRect.center();
    m_repriseSlurArcDirection = note->chord()->up() ? 1.0 : -1.0;
    const auto beginPosition = event->position();
    const auto expectedTick = note->tick();
    const auto expectedTrack = note->track();
    const int expectedPitch = note->pitch();
    const int shape = m_repriseToolModel->activeShape();
    const std::weak_ptr<INotation> weakNotation = m_notation;
    // Capture values and a weak notation, never a Note*/Chord* across events.
    m_repriseSlurCommit = [this, beginPosition, expectedTick, expectedTrack, expectedPitch, shape, weakNotation](const QPointF& endPosition) {
        const auto notation = weakNotation.lock();
        if (!notation || notation != m_notation || !repriseSlurInputAvailable()
            || m_repriseToolModel->activeShape() != shape) {
            return false;
        }

        auto first = repriseNormalNote(m_inputController->hitElementAtViewPosition(beginPosition));
        if (!first) {
            const qreal scaling = std::max(currentScaling(), 0.001);
            first = repriseNearestNormalNote(notation, toLogical(beginPosition), muse::nidx, expectedTrack,
                                             REPRISE_NOTE_START_RADIUS_PX / scaling);
            if (!first) {
                first = repriseNearestNormalNote(notation, toLogical(beginPosition), muse::nidx, expectedTrack);
            }
        }
        if (!first || first->tick() != expectedTick || first->track() != expectedTrack || first->pitch() != expectedPitch) {
            return false;
        }

        if (!repriseSmartShape::isNoteAnchoredShape(shape)) {
            const std::optional<Fraction> endTick = repriseTimeAnchorTick(notation, toLogical(endPosition), first->staffIdx());
            if (!endTick || *endTick <= first->tick()) {
                return false;
            }
            if (repriseSmartShape::isHairpinShape(shape)) {
                return repriseSmartShape::addHairpin(notation, shape, first->tick(), *endTick, first->track());
            }
            return repriseSmartShape::addTimeShape(notation, shape, first->staffIdx(), first->track(), first->tick(), *endTick);
        }

        const bool slurShape = repriseSmartShape::isSlurShape(shape);
        const qreal scaling = std::max(currentScaling(), 0.001);
        const double targetRadius = REPRISE_NOTE_TARGET_RADIUS_PX / scaling;
        auto last = repriseNearestNormalNote(notation, toLogical(endPosition), first->staffIdx(), muse::nidx,
                                             targetRadius, first);

        // Finale permits a slur started on the last note to end freely to the
        // right. Keep this exception only for the two slur presets.
        if (slurShape && !last
            && endPosition.x() > m_repriseSlurPreviewStart.x()
            && repriseIsLastNormalNoteOnStaff(notation, first)) {
            const PointF endOffset = toLogical(endPosition) - first->canvasBoundingRect().center();
            return endOffset.x() > 0.0 && repriseSmartShape::addFreeEndSlur(notation, shape, first, endOffset);
        }

        if (!last && slurShape) {
            last = repriseNearestNormalNote(notation, toLogical(endPosition), first->staffIdx(), muse::nidx,
                                            std::numeric_limits<double>::max(), first);
        }
        if (!last || first == last || last->staffIdx() != first->staffIdx() || first->tick() == last->tick()) {
            return false;
        }

        return repriseSmartShape::addNoteShape(notation, shape, first, last);
    };
    updateRepriseSlurPreview(event->position());
    return true;
}

void AbstractNotationPaintView::clearRepriseSlurPreview()
{
    const bool wasVisible = m_repriseSlurPreviewVisible;
    m_repriseSlurPreviewVisible = false;
    m_repriseSlurPreviewValid = false;
    m_repriseSlurStartRect = {};
    m_repriseSlurTargetRect = {};
    if (wasVisible) {
        scheduleRedraw();
    }
}

void AbstractNotationPaintView::updateRepriseSlurPreview(const QPointF& position)
{
    if (!m_reprisePointer.pending() || !m_repriseToolModel || !m_repriseToolModel->operationActive()) {
        clearRepriseSlurPreview();
        return;
    }
    if (!repriseSlurInputAvailable()) {
        cancelRepriseSlur();
        return;
    }
    m_repriseSlurPreviewVisible = true;
    m_repriseSlurPreviewValid = false;
    m_repriseSlurPreviewEnd = position;
    m_repriseSlurPreviewReversed = position.x() < m_repriseSlurPreviewStart.x();
    m_repriseSlurTargetRect = {};

    const int shape = m_repriseToolModel->activeShape();
    const qreal scaling = std::max(currentScaling(), 0.001);
    auto first = repriseNearestNormalNote(m_notation, toLogical(m_repriseSlurPressPosition), muse::nidx, muse::nidx,
                                          REPRISE_NOTE_START_RADIUS_PX / scaling);
    if (!first) {
        first = repriseNearestNormalNote(m_notation, toLogical(m_repriseSlurPressPosition));
    }
    if (!first) {
        scheduleRedraw();
        return;
    }

    m_repriseSlurStartRect = fromLogical(first->canvasBoundingRect()).toQRectF();
    m_repriseSlurPreviewStart = m_repriseSlurStartRect.center();

    if (!repriseSmartShape::isNoteAnchoredShape(shape)) {
        const std::optional<Fraction> endTick = repriseTimeAnchorTick(m_notation, toLogical(position), first->staffIdx());
        m_repriseSlurPreviewValid = endTick && *endTick > first->tick();
        scheduleRedraw();
        return;
    }

    const bool slurShape = repriseSmartShape::isSlurShape(shape);

    // Preview and release share the same note-capture resolver. The start note
    // (and every note at its tick) is explicitly excluded.
    auto last = repriseNearestNormalNote(m_notation, toLogical(position), first->staffIdx(), muse::nidx,
                                         REPRISE_NOTE_TARGET_RADIUS_PX / scaling, first);
    const bool freeEnd = slurShape
                         && last == nullptr
                         && position.x() > m_repriseSlurPreviewStart.x()
                         && repriseIsLastNormalNoteOnStaff(m_notation, first);
    if (first && last && first != last && first->staffIdx() == last->staffIdx() && first->tick() != last->tick()) {
        m_repriseSlurTargetRect = fromLogical(last->canvasBoundingRect()).toQRectF();
        m_repriseSlurPreviewEnd = m_repriseSlurTargetRect.center();
        m_repriseSlurPreviewReversed = first->tick() > last->tick();
        m_repriseSlurPreviewValid = true;
    } else if (slurShape || freeEnd) {
        // Being between notes is not an error for a phrase slur. A target ring
        // appears only while a real destination note is captured.
        m_repriseSlurPreviewValid = true;
    }
    scheduleRedraw();
}

void AbstractNotationPaintView::paintRepriseSlurPreview(QPainter* painter)
{
    if (!m_repriseSlurPreviewVisible || !m_repriseToolModel || !m_repriseToolModel->operationActive()) {
        return;
    }

    repriseSmartShapePreview::paint(painter, m_repriseToolModel->activeShape(),
                                    engravingConfiguration()->selectionColor().toQColor(),
                                    m_repriseSlurPreviewStart, m_repriseSlurPreviewEnd,
                                    m_repriseSlurArcDirection, m_repriseSlurPreviewValid,
                                    m_repriseSlurStartRect, m_repriseSlurTargetRect);
}

bool AbstractNotationPaintView::canScrubPlayback() const
{
    if (!isInited() || !m_notation || !notationPlayback() || !hasActiveFocus() || !usesRepriseEditing()
        || m_isContextMenuOpen || m_currentElementPopupType != PopupModelType::TYPE_UNDEFINED
        || m_reprisePointer.ownsPointer() || playbackController()->isPlaying()) {
        return false;
    }
    const auto interaction = notationInteraction();
    return interaction && !interaction->isTextEditingStarted() && !interaction->isEditingElement();
}

bool AbstractNotationPaintView::isPlaybackScrubShortcut(const QKeyEvent* event) const
{
    return event->key() == Qt::Key_Space && event->modifiers() == Qt::ControlModifier
           && (m_playbackScrubHeld || canScrubPlayback());
}

void AbstractNotationPaintView::scrubPlaybackAt(const QPointF& position)
{
    const PointF scorePosition = toLogical(position);
    const auto* score = m_notation->elements()->msScore();
    const auto* measure = contains(position) ? score->searchMeasure(scorePosition) : nullptr;
    constexpr auto segmentType = engraving::SegmentType::ChordRest;
    const auto* segment = measure ? measure->searchSegment(scorePosition.x() - measure->canvasPos().x(),
                                                           segmentType, 0, score->ntracks()) : nullptr;
    if (!segment) {
        clearPlaybackScrubNotes();
        return;
    }

    const Fraction tick = segment->tick();
    if (tick.ticks() == m_playbackScrubTick) {
        return;
    }
    m_playbackScrubTick = tick.ticks();

    // A voice can still be sounding when another voice reaches a new segment.
    std::vector<const EngravingItem*> notes;
    std::vector<bool> foundVoice(score->ntracks(), false);
    for (const auto* current = segment; current; current = current->prev(segmentType)) {
        if (!current->enabled()) {
            continue;
        }
        for (engraving::track_idx_t track = 0; track < score->ntracks(); ++track) {
            const auto* item = current->element(track);
            if (foundVoice[track] || !item) {
                continue;
            }
            foundVoice[track] = true;
            if (!item->isChord() || engraving::toChord(item)->endTick() <= tick) {
                continue;
            }
            for (const auto* note : engraving::toChord(item)->notes()) {
                if (note->play()) {
                    notes.push_back(note);
                }
            }
        }
    }

    // The gesture supplies the note-off by flushing the preview tracks.
    constexpr muse::mpe::duration_t heldDuration = 60LL * 60 * 1'000'000;
    notationPlayback()->triggerEventsForItems(notes, heldDuration, true, true);
}

void AbstractNotationPaintView::clearPlaybackScrubNotes()
{
    if (m_playbackScrubTick != -1) {
        notationPlayback()->triggerEventsForItems({}, 0, true, true);
        m_playbackScrubTick = -1;
    }
}

void AbstractNotationPaintView::endPlaybackScrub()
{
    if (m_playbackScrubHeld) {
        clearPlaybackScrubNotes();
        m_playbackScrubHeld = false;
        setAcceptHoverEvents(isNoteEnterMode());
    }
}

bool AbstractNotationPaintView::canRouteRepriseEscape(const QKeyEvent* event) const
{
    if (event->key() != Qt::Key_Escape || event->modifiers() != Qt::NoModifier
        || !isInited() || !m_notation || !hasActiveFocus() || !isMainView()
        || publishMode() || m_readonly || !m_repriseToolModel || !m_repriseToolModel->enabled()
        || m_isContextMenuOpen || m_currentElementPopupType != PopupModelType::TYPE_UNDEFINED) {
        return false;
    }
    return !m_inputController->hasNativeEscapePriority(m_reprisePointer.ownsPointer());
}

mu::engraving::Note* AbstractNotationPaintView::repriseAccidentalStepNote(const QKeyEvent* event) const
{
    if ((event->key() != Qt::Key_Plus && event->key() != Qt::Key_Minus)
        || event->modifiers() != Qt::KeypadModifier
        || !isInited() || !m_notation || !hasActiveFocus() || !usesRepriseEditing()
        || m_isContextMenuOpen || m_currentElementPopupType != PopupModelType::TYPE_UNDEFINED) {
        return nullptr;
    }

    const auto interaction = m_notation->interaction();
    if (interaction->isTextEditingStarted() || interaction->isEditingElement()) {
        return nullptr;
    }

    const auto selection = interaction->selection();
    auto* item = selection->element();
    auto* note = item && item->isNote() ? engraving::toNote(item) : nullptr;
    return note && note->staff() && note->staff()->isPitchedStaff(note->tick())
           && !engraving::Accidental::isMicrotonal(note->accidentalType()) ? note : nullptr;
}

bool AbstractNotationPaintView::isRepriseKeypadTie(const QKeyEvent* event) const
{
    if ((event->key() != Qt::Key_Slash && event->key() != Qt::Key_division)
        || event->modifiers() != Qt::KeypadModifier || !isInited() || !hasActiveFocus()
        || !usesRepriseEditing() || m_isContextMenuOpen || m_currentElementPopupType != PopupModelType::TYPE_UNDEFINED) {
        return false;
    }

    const auto interaction = notationInteraction();
    return interaction && !interaction->isTextEditingStarted() && !interaction->isEditingElement();
}

void AbstractNotationPaintView::keyPressEvent(QKeyEvent* event)
{
    if (isPlaybackScrubShortcut(event)) {
        if (!event->isAutoRepeat() && !m_playbackScrubHeld) {
            m_playbackScrubHeld = true;
            setAcceptHoverEvents(true);
            scrubPlaybackAt(mapFromGlobal(QCursor::pos()));
        }
        event->accept();
        return;
    }

    if (speedyEntryActive() && hasActiveFocus() && !notationInteraction()->isTextEditingStarted()) {
        if (isSpeedyEntryDirectCommand(event)) {
            const auto input = notationNoteInput();
            const auto& state = input->state();
            const int pitch = speedyKeyboardPitch(event);
            if (event->key() == Qt::Key_Backspace) {
                if (state.isValid() && !event->isAutoRepeat()) {
                    deleteSpeedyPitch(notation(), state);
                }
            } else if (state.isValid() && (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down)) {
                const auto staffIndex = state.staffIdx();
                if (event->key() == Qt::Key_Up && staffIndex > 0) {
                    input->setCurrentTrack((staffIndex - 1) * engraving::VOICES + state.voice());
                } else if (event->key() == Qt::Key_Down && staffIndex + 1 < state.staff()->score()->nstaves()) {
                    input->setCurrentTrack((staffIndex + 1) * engraving::VOICES + state.voice());
                }
            } else if (state.isValid() && (event->key() == Qt::Key_Apostrophe || event->key() == Qt::Key_QuoteDbl)) {
                const auto modifiers = event->modifiers() & ~Qt::KeypadModifier;
                const auto voice = state.voice();
                input->setCurrentVoice(modifiers == Qt::ShiftModifier
                                       ? (voice + 1) % engraving::VOICES
                                       : (voice == 0 ? 1 : 0));
            } else if (pitch >= 0 && state.isValid()) {
                engraving::NoteVal note(pitch + 12 * m_speedyOctaveOffset);
                note.tpc1 = note.tpc2 = engraving::pitch2tpc(note.pitch, engraving::Key::C, engraving::Prefer::NEAREST);
                input->setRestMode(false);
                input->setInputNotes({ note });
            } else if (pitch <= -2) {
                m_speedyOctaveOffset = pitch == -2 ? -1 : (pitch == -4 ? 1 : 0);
            } else if (state.isValid() && !state.notes().empty() && !event->isAutoRepeat()) {
                const auto* staff = state.staff();
                const auto& note = state.notes().front();
                const int line = engraving::noteValToLine(note, staff, state.tick());
                engraving::NoteInputParams params;
                params.step = engraving::absStep(line, staff->clef(state.tick()));
                input->addNote(params, NoteAddingMode::CurrentChord);
            }
            event->accept();
            return;
        }

        if (const char* action = speedyEntryAction(event)) {
            if (!event->isAutoRepeat() || event->key() == Qt::Key_BracketLeft || event->key() == Qt::Key_BracketRight) {
                const auto modifiers = event->modifiers() & ~Qt::KeypadModifier;
                if (modifiers == (Qt::ControlModifier | Qt::ShiftModifier)) {
                    const auto input = notationNoteInput();
                    input->setRestMode(true);
                    dispatcher()->dispatch(action);
                    input->setRestMode(false);
                } else if (!changeSpeedyChordDuration(notation(), notationNoteInput()->state().cr(), action)) {
                    dispatcher()->dispatch(action);
                }
            }
            event->accept();
            return;
        }
    }

    if (isRepriseKeypadTie(event)) {
        if (!event->isAutoRepeat()) {
            dispatcher()->dispatch("tie");
        }
        event->accept();
        return;
    }

    if (const auto* note = repriseAccidentalStepNote(event)) {
        static constexpr const char* ACCIDENTAL_ACTIONS[] = {
            "flat2-post", "flat-post", "nat-post", "sharp-post", "sharp2-post"
        };
        const int current = static_cast<int>(engraving::tpc2alter(note->tpc()));
        const int step = event->key() == Qt::Key_Plus ? 1 : -1;
        const int target = current + step;
        if (!event->isAutoRepeat() && target >= -2 && target <= 2) {
            dispatcher()->dispatch(ACCIDENTAL_ACTIONS[target + 2]);
        }
        event->accept();
        return;
    }

    if (canRouteRepriseEscape(event) && (m_repriseEscapeHeld || m_repriseToolModel->canCancel())) {
        if (!event->isAutoRepeat() && !m_repriseEscapeHeld) {
            m_repriseToolModel->cancel(false);
        }
        m_repriseEscapeHeld = true;
        m_lastAcceptedKey = -1;
        event->accept();
        return;
    }
    if (isInited()) {
        m_inputController->keyPressEvent(event);
    }

    if (event->key() == m_lastAcceptedKey) {
        // required to prevent Qt-Quick from changing focus on tab
        m_lastAcceptedKey = -1;
    }

    return;
}

void AbstractNotationPaintView::keyReleaseEvent(QKeyEvent* event)
{
    if (m_playbackScrubHeld && (event->key() == Qt::Key_Space || event->key() == Qt::Key_Control)) {
        if (!event->isAutoRepeat()) {
            endPlaybackScrub();
            m_lastAcceptedKey = -1;
        }
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape && m_repriseEscapeHeld) {
        if (!event->isAutoRepeat()) {
            m_repriseEscapeHeld = false;
        }
        event->accept();
        return;
    }
    if (isInited()) {
        m_inputController->keyReleaseEvent(event);
    }
}

bool AbstractNotationPaintView::event(QEvent* event)
{
    if (event->type() == QEvent::FocusOut) {
        endPlaybackScrub();
        cancelRepriseSlur();
        m_repriseEscapeHeld = false;
    }
    if (!isInited()) {
        return QQuickPaintedItem::event(event);
    }

    QEvent::Type eventType = event->type();
    auto keyEvent = dynamic_cast<QKeyEvent*>(event);

    bool isContextMenuEvent = ((eventType == QEvent::ShortcutOverride && keyEvent->key() == Qt::Key_Menu)
                               || eventType == QEvent::Type::ContextMenu) && hasFocus();

    if (isContextMenuEvent) {
        QContextMenuEvent* contextMenuEvent = dynamic_cast<QContextMenuEvent*>(event);
        if (contextMenuEvent && contextMenuEvent->reason() == QContextMenuEvent::Mouse
            && m_inputController->ignoreNextMouseContextMenuEvent()) {
            return true;
        }
        QPointF pos = contextMenuEvent && !contextMenuEvent->pos().isNull()
                      ? mapFromGlobal(contextMenuEvent->globalPos())
                      : fromLogical(m_inputController->selectionElementPos()).toQPointF();
        showContextMenu(m_inputController->selectionType(), pos);
        return true;
    } else if (eventType == QEvent::Type::ShortcutOverride) {
        bool shouldOverrideShortcut = shortcutOverride(keyEvent);

        if (shouldOverrideShortcut) {
            m_lastAcceptedKey = keyEvent->key();
            keyEvent->accept();
            return true;
        }
    }

    return QQuickPaintedItem::event(event);
}

void AbstractNotationPaintView::inputMethodEvent(QInputMethodEvent* event)
{
    if (isInited()) {
        m_inputController->inputMethodEvent(event);
    }
}

QVariant AbstractNotationPaintView::inputMethodQuery(Qt::InputMethodQuery query) const
{
    if (isInited() && m_inputController->canHandleInputMethodQuery(query)) {
        return m_inputController->inputMethodQuery(query);
    }

    return QQuickPaintedItem::inputMethodQuery(query);
}

void AbstractNotationPaintView::dragEnterEvent(QDragEnterEvent* event)
{
    if (isInited()) {
        m_inputController->dragEnterEvent(event);
    }
}

void AbstractNotationPaintView::dragLeaveEvent(QDragLeaveEvent* event)
{
    if (isInited()) {
        m_inputController->dragLeaveEvent(event);
    }
}

void AbstractNotationPaintView::dragMoveEvent(QDragMoveEvent* event)
{
    if (isInited()) {
        m_inputController->dragMoveEvent(event);
    }
}

void AbstractNotationPaintView::dropEvent(QDropEvent* event)
{
    if (isInited()) {
        m_inputController->dropEvent(event);
    }
}

void AbstractNotationPaintView::setNotation(INotationPtr notation)
{
    endPlaybackScrub();
    m_notation = notation;

    if (m_loadCalled) {
        m_continuousPanel->setNotation(m_notation);
        m_playbackCursor->setNotation(m_notation);
        m_loopInMarker->setNotation(m_notation);
        m_loopOutMarker->setNotation(m_notation);
    }
}

void AbstractNotationPaintView::setReadonly(bool readonly)
{
    m_readonly = readonly;
    if (m_inputController) {
        m_inputController->setReadonly(m_readonly);
    }
}

void AbstractNotationPaintView::clear()
{
    Transform oldMatrix = m_matrix;
    m_matrix = Transform();
    m_previousHorizontalScrollPosition = 0;
    m_previousVerticalScrollPosition = 0;
    m_shadowNoteRect = RectF();
    onMatrixChanged(oldMatrix, m_matrix, false);
}

qreal AbstractNotationPaintView::width() const
{
    return QQuickPaintedItem::width();
}

qreal AbstractNotationPaintView::height() const
{
    return QQuickPaintedItem::height();
}

PointF AbstractNotationPaintView::toLogical(const PointF& point) const
{
    return m_matrix.inverted().map(point);
}

PointF AbstractNotationPaintView::toLogical(const QPointF& point) const
{
    return toLogical(PointF::fromQPointF(point));
}

RectF AbstractNotationPaintView::toLogical(const RectF& rect) const
{
    return m_matrix.inverted().map(rect);
}

PointF AbstractNotationPaintView::fromLogical(const PointF& point) const
{
    return m_matrix.map(point);
}

RectF AbstractNotationPaintView::fromLogical(const RectF& rect) const
{
    return m_matrix.map(rect);
}

bool AbstractNotationPaintView::isInited() const
{
    if (qFuzzyIsNull(width()) || qFuzzyIsNull(height())) {
        return false;
    }

    return notation() != nullptr;
}

void AbstractNotationPaintView::onPlayingChanged()
{
    TRACEFUNC;

    if (!notationPlayback()) {
        return;
    }

    bool isPlaying = playbackController()->isPlaying();
    m_playbackCursor->setVisible(isPlaying);

    if (m_playbackCursorItem) {
        m_playbackCursorItem->setVisible(isPlaying);
    }

    m_autoScrollEnabled = true;
    m_enableAutoScrollTimer.stop();

    if (isPlaying) {
        endPlaybackScrub();
        selectRepriseTool(RepriseToolModel::Selection);
        audio::secs_t pos = globalContext()->playbackState()->playbackPosition();
        muse::midi::tick_t tick = notationPlayback()->secToTick(pos);
        movePlaybackCursor(tick);
    } else {
        scheduleRedraw();
    }
}

void AbstractNotationPaintView::movePlaybackCursor(muse::midi::tick_t tick)
{
    TRACEFUNC;

    if (!notationPlayback()) {
        return;
    }

    RectF oldCursorRect = m_playbackCursor->rect();
    m_playbackCursor->move(tick);
    const RectF& newCursorRect = m_playbackCursor->rect();

    if (newCursorRect != oldCursorRect) {
        onPlaybackCursorRectChanged();
    }

    if (!m_playbackCursor->visible() || newCursorRect.isNull()) {
        return;
    }

    if (m_isAutomaticallyPanEnabled) {
        if ((notation()->viewMode() == engraving::LayoutMode::LINE) && m_isSmoothPanningEnabled
            && adjustCanvasPositionSmoothPan(newCursorRect)) {
            return;
        }

        if (m_autoScrollEnabled) {
            bool adjustVertically = needAdjustCanvasVerticallyWhilePlayback(newCursorRect);
            if (adjustCanvasPosition(newCursorRect, adjustVertically)) {
                return;
            }
        }
    }
}

bool AbstractNotationPaintView::needAdjustCanvasVerticallyWhilePlayback(const RectF& cursorRect)
{
    if (!viewport().intersects(cursorRect)) {
        return true;
    }

    const Page* page = pageByPoint(cursorRect.topRight());
    if (!page) {
        return false;
    }

    int nonEmptySystemCount = 0;

    for (const System* system : page->systems()) {
        if (!system->staves().empty()) {
            nonEmptySystemCount++;
        }
    }

    return nonEmptySystemCount > 1;
}

void AbstractNotationPaintView::onPlaybackCursorRectChanged()
{
    if (!m_playbackCursor || !m_playbackCursorItem) {
        return;
    }

    QRectF cursorRect = fromLogical(m_playbackCursor->rect()).toQRectF();
    cursorRect.setWidth(std::max(std::round(cursorRect.width()), 1.0)); // makes it move more smoothly

    QRectF viewRect(0.0, 0.0, width(), height());
    QRectF newRect = cursorRect.intersected(viewRect);

    m_playbackCursorItem->setX(newRect.x());
    m_playbackCursorItem->setY(newRect.y());
    m_playbackCursorItem->setSize(newRect.size());
}

const Page* AbstractNotationPaintView::pageByPoint(const PointF& point) const
{
    INotationElementsPtr elements = notationElements();
    return elements ? elements->pageByPoint(point) : nullptr;
}

PointF AbstractNotationPaintView::alignToCurrentPageBorder(const RectF& showRect, const PointF& pos) const
{
    TRACEFUNC;

    PointF result = pos;
    const Page* page = pageByPoint(showRect.topLeft());
    if (!page) {
        return result;
    }

    RectF viewRect = viewport();

    if (result.x() < page->x() || viewRect.width() >= page->width()) {
        result.setX(page->x());
    } else if (viewRect.width() < page->width() && viewRect.width() + pos.x() > page->width() + page->x()) {
        result.setX((page->width() + page->x()) - viewRect.width());
    }
    if (result.y() < page->y() || viewRect.height() >= page->height()) {
        result.setY(page->y());
    } else if (viewRect.height() < page->height() && viewRect.height() + pos.y() > page->height() + page->y()) {
        result.setY((page->height() + page->y()) - viewRect.height());
    }

    return result;
}

bool AbstractNotationPaintView::publishMode() const
{
    return m_publishMode;
}

void AbstractNotationPaintView::setPublishMode(bool arg)
{
    if (m_publishMode == arg) {
        return;
    }

    m_publishMode = arg;
    emit publishModeChanged();
}

bool AbstractNotationPaintView::isMainView() const
{
    return m_isMainView;
}

void AbstractNotationPaintView::setIsMainView(bool isMainView)
{
    if (m_isMainView == isMainView) {
        return;
    }

    m_isMainView = isMainView;
    emit isMainViewChanged(m_isMainView);
}

void AbstractNotationPaintView::setPlaybackCursorItem(QQuickItem* cursor)
{
    if (m_playbackCursorItem == cursor) {
        return;
    }

    m_playbackCursorItem = cursor;

    if (m_playbackCursorItem) {
        m_playbackCursorItem->setVisible(playbackController()->isPlaying());
        m_playbackCursorItem->setEnabled(false); // ignore mouse & keyboard events
        m_playbackCursorItem->setProperty("color", notationConfiguration()->playbackCursorColor());

        connect(m_playbackCursorItem, &QObject::destroyed, this, [this]() {
            m_playbackCursorItem = nullptr;
        });
    }
}
