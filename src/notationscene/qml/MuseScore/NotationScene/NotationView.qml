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

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Muse.Ui
import Muse.UiComponents
import MuseScore.NotationScene
import MuseScore.Braille
import MuseScore.Playback

import "internal"

FocusScope {
    id: root

    property alias name: notationView.objectName
    property alias publishMode: notationView.publishMode

    property alias paintView: notationView

    property alias isNavigatorVisible: notationNavigator.visible
    property alias isBraillePanelVisible: brailleViewLoader.active
    property alias isMainView: notationView.isMainView

    property alias defaultNavigationControl: fakeNavCtrl

    readonly property alias navigationSection: navSec
    property RepriseToolModel repriseToolModel: defaultRepriseToolModel

    signal repriseCatalogRequested()
    signal repriseStaffRequested()
    signal repriseArticulationRequested()
    signal repriseTimeSignatureRequested()
    signal repriseClefRequested()
    signal repriseKeySignatureRequested()
    signal repriseRepeatRequested(bool repeatEnd)
    signal repriseMeasureRequested()
    signal repriseExpressionRequested()

    property Component repriseCollectionsComponent: null
    property bool showRepriseCollections: false
    readonly property bool reprisePanelExpanded: root.showRepriseCollections
                                                || root.repriseToolModel.activeTool === RepriseToolModel.SmartShape
    readonly property bool reprisePanelAvailable: root.isMainView && !root.publishMode && root.repriseToolModel.enabled
                                                  && (!notationView.noteInputActive || root.showRepriseCollections)
    readonly property real repriseViewportWidth: root.width
    readonly property int reprisePanelRightInset: 16
    readonly property int reprisePanelContentGap: 8
    // Keep a usable score when the notation dock is narrowed by other panels.
    readonly property int repriseMinimumScoreWidth: 240

    RepriseToolModel {
        id: defaultRepriseToolModel
    }

    RepriseArticulationInputController {
        view: notationView
        toolModel: root.repriseToolModel
        onArticulationRequested: root.repriseArticulationRequested()
    }

    RepriseTimeSignatureInputController {
        view: notationView
        toolModel: root.repriseToolModel
        onTimeSignatureRequested: root.repriseTimeSignatureRequested()
    }

    RepriseClefInputController {
        view: notationView
        toolModel: root.repriseToolModel
        onClefRequested: root.repriseClefRequested()
    }

    RepriseMeasureInputController {
        targetTool: RepriseToolModel.KeySignature
        view: notationView
        toolModel: root.repriseToolModel
        onMeasureRequested: root.repriseKeySignatureRequested()
    }

    RepriseMeasureInputController {
        targetTool: RepriseToolModel.Repeat
        view: notationView
        toolModel: root.repriseToolModel
        onMeasureRequested: repeatEnd => root.repriseRepeatRequested(repeatEnd)
    }

    RepriseMeasureInputController {
        targetTool: RepriseToolModel.Measure
        view: notationView
        toolModel: root.repriseToolModel
        onMeasureRequested: root.repriseMeasureRequested()
    }

    RepriseExpressionInputController {
        view: notationView
        toolModel: root.repriseToolModel
        onExpressionRequested: root.repriseExpressionRequested()
    }

    // Declared last so its event filter gets first refusal on Reprise
    // Selection's one special case; inactive tool adapters still delegate.
    RepriseSelectionInputController {
        view: notationView
        toolModel: root.repriseToolModel
    }

    function closeRepriseCollections() {
        root.showRepriseCollections = false
        root.focusRepriseScore()
    }

    function focusRepriseScore() {
        root.showRepriseCollections = false
        fakeNavCtrl.requestActive()
        notationView.forceFocusIn()
    }

    Loader {
        id: repriseToolsPanel
        anchors.right: parent.right
        anchors.rightMargin: root.reprisePanelRightInset
        // The SplitView already follows tab visibility and page resize.
        // Anchor the overlay to its actual geometry, rather than estimating
        // an available height from the outer window's bottom edge.
        anchors.top: parent.top
        anchors.topMargin: splitView.y
        z: 10
        width: active
               ? Math.min(root.showRepriseCollections ? 280 : 208,
                          Math.max(0, root.repriseViewportWidth - 2 * root.reprisePanelRightInset))
               : 0
        height: active ? Math.max(0, splitView.height) : 0
        active: root.reprisePanelAvailable && root.reprisePanelExpanded
        sourceComponent: root.showRepriseCollections && root.repriseCollectionsComponent
                         ? root.repriseCollectionsComponent : repriseToolContext
        onLoaded: {
            if (root.showRepriseCollections && item && typeof item.activate === "function") {
                item.activate()
            }
        }
    }

    Component {
        id: repriseToolContext
        RepriseToolPanel {
            toolModel: root.repriseToolModel
            onScoreFocusRequested: root.focusRepriseScore()
        }
    }

    NavigationSection {
        id: navSec
        name: "NotationView"
        order: 5
        enabled: root.visible
    }

    QtObject {
        id: prv
        readonly property int scrollbarMargin: 4
    }

    NotationContextMenuModel {
        id: contextMenuModel
    }

    Component.onCompleted: {
        notationView.load()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        NotationSwitchPanel {
            id: tabPanel
            Layout.fillWidth: true

            navigationPanel.section: navSec
            navigationPanel.order: 1
        }

        SeparatorLine {
            id: tabSeparator
            visible: tabPanel.visible
        }

        SplitView {
            id: splitView
            // Reserve a gutter for expanded tool panels when there is room.
            Layout.rightMargin: repriseToolsPanel.active
                                && root.repriseViewportWidth >= repriseToolsPanel.width
                                                 + root.reprisePanelRightInset
                                                 + root.reprisePanelContentGap
                                                 + root.repriseMinimumScoreWidth
                                ? repriseToolsPanel.width + root.reprisePanelRightInset
                                  + root.reprisePanelContentGap
                                : 0
            Layout.fillWidth: true
            Layout.fillHeight: true

            orientation: notationNavigator.orientation === Qt.Horizontal ? Qt.Vertical : Qt.Horizontal

            StyledViewScrollAndZoomArea {
                SplitView.fillWidth: true
                SplitView.fillHeight: true

                horizontalScrollbarSize: notationView.horizontalScrollbarSize
                startHorizontalScrollPosition: notationView.startHorizontalScrollPosition

                verticalScrollbarSize: notationView.verticalScrollbarSize
                startVerticalScrollPosition: notationView.startVerticalScrollPosition

                NotationPaintView {
                    id: notationView
                    anchors.fill: parent
                    repriseToolModel: root.repriseToolModel

                    property NavigationPanel navigationPanel: NavigationPanel {
                        name: "ScoreView"
                        section: navSec
                        enabled: notationView.enabled && notationView.visible
                        direction: NavigationPanel.Both
                        order: tabPanel.navigationPanel.order + 1
                    }

                    NavigationControl {
                        id: fakeNavCtrl
                        name: "Score"
                        enabled: notationView.enabled && notationView.visible

                        panel: notationView.navigationPanel
                        order: 1

                        onActiveChanged: {
                            if (fakeNavCtrl.active) {
                                notationView.forceFocusIn()

                                if (notationView.navigationPanel.highlight) {
                                    notationView.selectOnNavigationActive()
                                }
                            } else {
                                notationView.focus = false
                            }
                        }
                    }

                    NavigationFocusBorder {
                        navigationCtrl: fakeNavCtrl
                        drawOutsideParent: false
                    }

                    Rectangle {
                        id: playbackCursor

                        Component.onCompleted: {
                            notationView.setPlaybackCursorItem(playbackCursor)
                        }
                    }

                    onActiveFocusRequested: {
                        fakeNavCtrl.requestActive()
                    }

                    onShowContextMenuRequested: function (elementType, viewPos) {
                        contextMenuModel.loadItems(elementType)
                        contextMenuLoader.show(viewPos, contextMenuModel.items)
                    }

                    onHideContextMenuRequested: {
                        contextMenuLoader.close()
                    }

                    onShowElementPopupRequested: function (popupType) {
                        popUpLoader.updateShow(popupType);
                    }

                    onHideElementPopupRequested: {
                        popUpLoader.updateShow(AbstractElementPopupModel.TYPE_UNDEFINED);
                    }

                    onViewportChanged: {
                        notationNavigator.setCursorRect(viewport)
                    }

                    ContextMenuLoader {
                        id: contextMenuLoader

                        onHandleMenuItem: function(itemId) {
                            contextMenuModel.handleMenuItem(itemId)
                        }

                        onOpened: notationView.onContextMenuIsOpenChanged(true)
                        onClosed: notationView.onContextMenuIsOpenChanged(false)
                    }

                    ElementPopupLoader {
                        id: popUpLoader

                        notationViewNavigationSection: navSec
                        navigationOrderStart: notationView.navigationPanel.order + 1

                        property int popupType: AbstractElementPopupModel.TYPE_UNDEFINED
                        property bool updateShowScheduled: false

                        function updateShow(popupType) {
                            this.popupType = popupType;
                            if (!updateShowScheduled) {
                                Qt.callLater(doUpdateShow)
                                updateShowScheduled = true;
                            }
                        }

                        function doUpdateShow() {
                            updateShowScheduled = false;

                            if (popupType !== AbstractElementPopupModel.TYPE_UNDEFINED) {
                                show(popupType);
                            } else {
                                close();
                            }
                        }

                        onOpened: function(popupType) {
                            notationView.onElementPopupIsOpenChanged(popupType)
                        }

                        onClosed: {
                            notationView.onElementPopupIsOpenChanged(AbstractElementPopupModel.TYPE_UNDEFINED)
                        }
                    }

                    NotationRegionsBeingProcessedView {
                        notationViewRect: Qt.rect(notationView.x, notationView.y, notationView.width, notationView.height)
                        notationViewMatrix: notationView.matrix
                    }
                }

                onPinchToZoom: function(scale, pos) {
                    notationView.pinchToZoom(scale, pos)
                }

                onScrollHorizontal: function(newPos) {
                    notationView.scrollHorizontal(newPos)
                }

                onScrollVertical: function(newPos) {
                    notationView.scrollVertical(newPos)
                }
            }

            Loader {
                id: notationNavigator

                readonly property NotationNavigator navigatorItem: item as NotationNavigator

                property var orientation: navigatorItem?.orientation ?? Qt.Horizontal

                visible: false

                SplitView.preferredHeight: 100
                SplitView.preferredWidth: 100
                SplitView.minimumWidth: 30
                SplitView.minimumHeight: 30

                sourceComponent: visible ? navigatorComp : null

                function setCursorRect(viewport) {
                    if (navigatorItem) {
                        navigatorItem.setCursorRect(viewport)
                    }
                }
            }

            Loader {
                id: brailleViewLoader

                readonly property int navigationOrder: popUpLoader.navigationOrderEnd + 1

                active: false
                visible: active

                SplitView.fillWidth: true
                SplitView.preferredHeight: 50
                SplitView.minimumHeight: 30

                sourceComponent: BrailleView {
                    navigationPanel.section: navSec
                    navigationPanel.order: brailleViewLoader.navigationOrder

                    navigationPanel.onActiveChanged: function (active) {
                        if (active) {
                            notationView.navigationPanel.setActive(false)
                            fakeNavCtrl.setActive(false)
                        }
                    }
                }
            }

            Component {
                id: navigatorComp

                NotationNavigator {

                    property bool isVertical: orientation === Qt.Vertical

                    objectName: root.name + "Navigator"

                    Component.onCompleted: {
                        load()
                    }

                    onMoveNotationRequested: function(dx, dy) {
                        notationView.moveCanvas(dx, dy)
                    }
                }
            }

            handle: Rectangle {
                id: resizingHandle

                implicitWidth: 4
                implicitHeight: 4

                color: ui.theme.strokeColor

                states: [
                    State {
                        name: "PRESSED"
                        when: resizingHandle.SplitHandle.pressed
                        PropertyChanges {
                            target: resizingHandle
                            opacity: ui.theme.accentOpacityHit
                        }
                    },
                    State {
                        name: "HOVERED"
                        when: resizingHandle.SplitHandle.hovered
                        PropertyChanges {
                            target: resizingHandle
                            opacity: ui.theme.accentOpacityHover
                        }
                    }
                ]
            }
        }

        Loader {
            id: repriseToolHints

            Layout.fillWidth: true
            Layout.preferredHeight: item ? item.implicitHeight : 0
            active: root.reprisePanelAvailable && !root.reprisePanelExpanded
            visible: active
            sourceComponent: repriseToolContext
        }

        SearchPopup {
            id: searchPopup

            Layout.fillWidth: true

            navigationPanel.section: navSec
            navigationOrderStart: brailleViewLoader.navigationOrder + 1

            onClosed: {
                fakeNavCtrl.requestActive()
            }
        }
    }
}
