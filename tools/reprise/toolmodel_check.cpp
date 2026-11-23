#include "notationscene/qml/MuseScore/NotationScene/reprisetoolmodel.h"

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QUrl>
#include <QtQml/qqml.h>
#include <cstdlib>
#include <iostream>
#include <memory>

using mu::notation::RepriseToolModel;
int checks = 0;
void require(bool condition, const char* message)
{
    ++checks;
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    QGuiApplication app(argc, argv);
    require(argc == 3, "panel path and Qt QML directory provided");
    qunsetenv("REPRISE_TOOLS");
    RepriseToolModel defaultModel;
    require(defaultModel.enabled(), "Reprise tools are available on a standard launch");
    qputenv("REPRISE_TOOLS", "0");
    RepriseToolModel disabled;
    require(!disabled.enabled() && !disabled.selectTool(RepriseToolModel::SmartShape), "disabled model cannot activate tools");
    require(!disabled.selectTool(RepriseToolModel::Articulation), "disabled model cannot activate Articulation");
    require(!disabled.selectTool(RepriseToolModel::TimeSignature), "disabled model cannot activate Time Signature");
    require(!disabled.selectTool(RepriseToolModel::Clef), "disabled model cannot activate Clef");
    require(!disabled.beginOperation() && !disabled.cancel(false) && !disabled.canCancel(), "disabled model cannot consume input");
    qputenv("REPRISE_TOOLS", "1");
    RepriseToolModel model;
    RepriseToolModel other;
    int signalCount = 0;
    QObject::connect(&model, &RepriseToolModel::stateChanged, [&signalCount]() { ++signalCount; });
    require(model.enabled() && !disabled.enabled(), "feature opt-in captured per instance");
    require(model.activeTool() == RepriseToolModel::Selection && !model.canCancel(), "neutral model defaults to Selection and delegates Escape");
    require(!model.selectTool(-1) && !model.selectTool(99) && signalCount == 0, "invalid identifiers rejected without signalCount");
    require(!model.beginOperation() && !model.cancel(false), "Selection cannot start an operation or consume Escape");
    require(model.selectTool(RepriseToolModel::SmartShape) && model.activeTool() == RepriseToolModel::SmartShape
            && model.canCancel() && signalCount == 1, "Smart Shape mapping and signal");
    require(other.activeTool() == RepriseToolModel::Selection, "view instances independent");
    require(model.selectTool(RepriseToolModel::SmartShape) && signalCount == 1, "redundant idle activation emits nothing");
    require(model.beginOperation() && model.operationActive() && signalCount == 2, "operation start signal");
    require(!model.beginOperation() && signalCount == 2, "nested operation rejected");
    require(!model.cancel(true) && model.operationActive() && signalCount == 2, "native owner preserves operation and signalCount");
    require(model.cancel(false) && !model.operationActive() && model.activeTool() == RepriseToolModel::SmartShape
            && signalCount == 3, "first Escape cancels only operation");
    require(model.cancel(false) && model.activeTool() == RepriseToolModel::Selection && signalCount == 4, "next Escape selects Selection");
    require(!model.cancel(false) && signalCount == 4, "neutral Escape delegates without signalCount");
    model.selectTool(RepriseToolModel::SmartShape);
    model.beginOperation();
    model.finishOperation();
    const int afterFinish = signalCount;
    model.finishOperation();
    require(!model.operationActive() && signalCount == afterFinish, "finish operation is idempotent");
    model.reset();
    const int afterReset = signalCount;
    model.reset();
    require(model.activeTool() == RepriseToolModel::Selection && signalCount == afterReset, "reset is idempotent");

    const int beforeArticulation = signalCount;
    require(model.selectTool(RepriseToolModel::Articulation)
            && model.activeTool() == RepriseToolModel::Articulation
            && model.canCancel() && signalCount == beforeArticulation + 1,
            "Articulation mapping and signal");
    require(model.cancel(false) && model.activeTool() == RepriseToolModel::Selection,
            "Articulation Escape returns to Selection");

    const int beforeTimeSignature = signalCount;
    require(model.selectTool(RepriseToolModel::TimeSignature)
            && model.activeTool() == RepriseToolModel::TimeSignature
            && model.canCancel() && signalCount == beforeTimeSignature + 1,
            "Time Signature mapping and signal");
    require(model.cancel(false) && model.activeTool() == RepriseToolModel::Selection,
            "Time Signature Escape returns to Selection");

    const int beforeClef = signalCount;
    require(model.selectTool(RepriseToolModel::Clef)
            && model.activeTool() == RepriseToolModel::Clef
            && model.canCancel() && signalCount == beforeClef + 1,
            "Clef mapping and signal");
    require(model.cancel(false) && model.activeTool() == RepriseToolModel::Selection,
            "Clef Escape returns to Selection");

    qmlRegisterType<RepriseToolModel>("MuseScore.NotationScene", 1, 0, "RepriseToolModel");
    QQmlEngine engine;
    engine.addImportPath(QString::fromLocal8Bit(argv[2]));
    QQmlComponent component(&engine, QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1])));
    if (component.isError()) {
        std::cerr << component.errorString().toStdString();
    }
    require(component.isReady(), "panel QML loads");
    std::unique_ptr<QObject> panel(component.createWithInitialProperties({ { "toolModel", QVariant::fromValue(&model) } }));
    if (!panel) {
        std::cerr << component.errorString().toStdString();
    }
    require(panel != nullptr && qobject_cast<QQuickItem*>(panel.get()), "panel creates with required model");
    QObject* title = panel->findChild<QObject*>("repriseContextTitle");
    require(title != nullptr, "panel context title present");
    require(title->property("text").toString() == QStringLiteral("Selection"), "initial panel context matches model");
    require(!panel->property("expanded").toBool(), "Selection context panel starts compact");
    require(panel->property("implicitHeight").toReal() == 112.0, "compact context panel uses compact height");

    require(model.selectTool(RepriseToolModel::SmartShape), "model activates Smart Shape for panel test");
    app.processEvents();
    require(title->property("text").toString() == QStringLiteral("Smart Shape"), "panel context follows Smart Shape model state");
    require(panel->property("expanded").toBool(), "Smart Shape expands the context panel");
    require(panel->property("implicitHeight").toReal() == 360.0, "expanded Smart Shape panel uses full content height");

    require(model.selectTool(RepriseToolModel::Articulation), "model activates Articulation for panel test");
    app.processEvents();
    require(title->property("text").toString() == QStringLiteral("Articulation"), "panel context follows Articulation model state");
    require(!panel->property("expanded").toBool(), "Articulation returns the context panel to compact mode");

    require(model.selectTool(RepriseToolModel::TimeSignature), "model activates Time Signature for panel test");
    app.processEvents();
    require(title->property("text").toString() == QStringLiteral("Time Signature"), "panel context follows Time Signature model state");
    require(!panel->property("expanded").toBool(), "Time Signature keeps the context panel compact");

    require(model.selectTool(RepriseToolModel::Clef), "model activates Clef for panel test");
    app.processEvents();
    require(title->property("text").toString() == QStringLiteral("Change Clef"), "panel context follows Clef model state");
    require(!panel->property("expanded").toBool(), "Clef keeps the context panel compact");

    require(model.selectTool(RepriseToolModel::SmartShape), "return to Smart Shape for shape controls");
    app.processEvents();
    require(!other.canCancel(), "panel changes remain local to its model");
    require(!disabled.selectShape(RepriseToolModel::Crescendo), "feature-off refuses shape changes");
    require(model.activeShape() == RepriseToolModel::Slur && other.activeShape() == RepriseToolModel::Slur,
            "new view models default to Slur");

    QObject* slur = panel->findChild<QObject*>("repriseSlurShape");
    QObject* crescendo = panel->findChild<QObject*>("repriseCrescendoShape");
    QObject* tabSlide = panel->findChild<QObject*>("repriseTabSlideShape");
    require(slur && crescendo && tabSlide, "regular shape controls instantiated in real panel");
    require(panel->findChild<QObject*>("repriseCustomLineShape") == nullptr, "unsupported Custom Line control is absent");
    require(QMetaObject::invokeMethod(crescendo, "clicked", Qt::DirectConnection), "Crescendo click routed by QML");
    app.processEvents();
    require(model.activeShape() == RepriseToolModel::Crescendo && crescendo->property("checked").toBool()
            && !slur->property("checked").toBool(), "shape binding follows QML choice");
    require(model.beginOperation(), "chosen shape starts an operation");
    const int shapeSignals = signalCount;
    require(model.selectShape(RepriseToolModel::Crescendo) && model.operationActive() && signalCount == shapeSignals,
            "same shape leaves pending operation intact");
    require(!model.selectShape(99) && model.operationActive() && signalCount == shapeSignals,
            "invalid shape cannot cancel a pending operation");
    require(QMetaObject::invokeMethod(tabSlide, "clicked", Qt::DirectConnection), "Tab slide click routed by QML");
    app.processEvents();
    require(model.activeShape() == RepriseToolModel::TabSlide && !model.operationActive()
            && tabSlide->property("checked").toBool(), "highest supported shape updates model and panel");
    require(QMetaObject::invokeMethod(slur, "clicked", Qt::DirectConnection), "Slur click routed by QML");
    app.processEvents();
    require(model.activeShape() == RepriseToolModel::Slur && !model.operationActive()
            && model.activeTool() == RepriseToolModel::SmartShape,
            "changing shape keeps Smart Shape active");
    require(slur->property("checked").toBool() && other.activeShape() == RepriseToolModel::Slur,
            "panel updated while other view stays independent");
    std::cout << "PASS: " << checks << " Qt model/panel checks\n";
}
