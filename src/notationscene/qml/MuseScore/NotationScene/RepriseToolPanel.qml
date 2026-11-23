/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Reprise Notation contributors
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Muse.Ui
import MuseScore.NotationScene

FocusScope {
    id: root

    required property RepriseToolModel toolModel
    signal scoreFocusRequested()
    implicitWidth: 208
    implicitHeight: expanded ? 360 : content.implicitHeight + 2 * content.anchors.margins
    readonly property bool expanded: root.toolModel.activeTool === RepriseToolModel.SmartShape
    readonly property bool darkTheme: ui.theme.isDark
    readonly property color panelColor: darkTheme ? darkAccent(ui.theme.accentColor) : ui.theme.accentColor
    readonly property color textColor: darkTheme ? "white" : readableTextColor(panelColor)

    function linearChannel(value: real): real {
        return value <= 0.04045 ? value / 12.92 : Math.pow((value + 0.055) / 1.055, 2.4)
    }

    function luminance(background: color): real {
        return 0.2126 * linearChannel(background.r)
             + 0.7152 * linearChannel(background.g)
             + 0.0722 * linearChannel(background.b)
    }

    function readableTextColor(background: color): color {
        return luminance(background) > 0.179 ? "black" : "white"
    }

    function darkAccent(accent: color): color {
        // Leave enough contrast for white text on the highlighted shape rows too.
        while (luminance(accent) > 0.10) {
            accent = Qt.darker(accent, 1.15)
        }
        return accent
    }

    // MuseScore's engraving objects provide all visible Smart Shape previews.
    RepriseSmartShapePreviewModel {
        id: nativePreviewModel
    }

    readonly property var shapeEntries: [
        { shape: RepriseToolModel.Slur, objectName: "repriseSlurShape", label: qsTr("Slur") },
        { shape: RepriseToolModel.DashedSlur, objectName: "repriseDashedSlurShape", label: qsTr("Dashed slur") },
        { shape: RepriseToolModel.Crescendo, objectName: "repriseCrescendoShape", label: qsTr("Crescendo") },
        { shape: RepriseToolModel.Decrescendo, objectName: "repriseDecrescendoShape", label: qsTr("Decrescendo") },
        { shape: RepriseToolModel.Trill, objectName: "repriseTrillShape", label: qsTr("Trill") },
        { shape: RepriseToolModel.TrillExtension, objectName: "repriseTrillExtensionShape", label: qsTr("Trill extension") },
        { shape: RepriseToolModel.Ottava8va, objectName: "repriseOttava8vaShape", label: qsTr("8va") },
        { shape: RepriseToolModel.Ottava15ma, objectName: "repriseOttava15maShape", label: qsTr("15ma") },
        { shape: RepriseToolModel.BracketBoth, objectName: "repriseBracketBothShape", label: qsTr("Bracket") },
        { shape: RepriseToolModel.BracketBothDashed, objectName: "repriseBracketBothDashedShape", label: qsTr("Dashed bracket") },
        { shape: RepriseToolModel.BracketEnd, objectName: "repriseBracketEndShape", label: qsTr("End bracket") },
        { shape: RepriseToolModel.BracketEndDashed, objectName: "repriseBracketEndDashedShape", label: qsTr("Dashed end bracket") },
        { shape: RepriseToolModel.Line, objectName: "repriseLineShape", label: qsTr("Line") },
        { shape: RepriseToolModel.DashedLine, objectName: "repriseDashedLineShape", label: qsTr("Dashed line") },
        { shape: RepriseToolModel.Glissando, objectName: "repriseGlissandoShape", label: qsTr("Glissando") },
        { shape: RepriseToolModel.TabSlide, objectName: "repriseTabSlideShape", label: qsTr("Tab slide") }
    ]

    function noteAnchoredShape(shape) {
        return shape === RepriseToolModel.Slur
               || shape === RepriseToolModel.DashedSlur
               || shape === RepriseToolModel.Glissando
               || shape === RepriseToolModel.TabSlide
    }

    function chooseShape(shape) {
        root.toolModel.selectShape(shape)
        root.scoreFocusRequested()
    }

    Rectangle {
        anchors.fill: parent
        radius: root.expanded ? 6 : 0
        color: root.panelColor
        border.width: root.expanded ? 0 : 1
        border.color: Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.35)
    }

    GridLayout {
        id: content

        anchors.fill: parent
        anchors.margins: root.expanded ? 12 : 6
        columns: root.expanded ? 1 : 2
        rowSpacing: 8
        columnSpacing: 12
        clip: true

        Label {
            objectName: "repriseContextTitle"
            text: root.toolModel.activeTool === RepriseToolModel.Measure
                  ? qsTr("Measure")
                  : root.toolModel.activeTool === RepriseToolModel.Repeat
                  ? qsTr("Repeat")
                  : root.toolModel.activeTool === RepriseToolModel.SmartShape
                  ? qsTr("Smart Shape")
                  : (root.toolModel.activeTool === RepriseToolModel.Articulation
                     ? qsTr("Articulation")
                     : (root.toolModel.activeTool === RepriseToolModel.TimeSignature
                        ? qsTr("Time Signature")
                        : (root.toolModel.activeTool === RepriseToolModel.Clef
                           ? qsTr("Change Clef")
                           : (root.toolModel.activeTool === RepriseToolModel.KeySignature
                              ? qsTr("Key Signature")
                              : (root.toolModel.activeTool === RepriseToolModel.Expression
                                 ? qsTr("Expression") : qsTr("Selection"))))))
            color: root.textColor
            font.bold: true
            wrapMode: Text.WordWrap
            Layout.fillWidth: root.expanded
            Layout.alignment: Qt.AlignTop
            Layout.maximumWidth: root.expanded ? root.width : root.width * 0.36
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0
            visible: root.expanded
            clip: true

            ColumnLayout {
                width: parent.width
                spacing: 2

                Repeater {
                    model: root.shapeEntries

                    delegate: Button {
                        id: shapeButton

                        required property var modelData
                        readonly property real highlightOpacity: root.darkTheme
                            ? (checked ? 0.12 : (hovered ? 0.06 : 0))
                            : (checked ? 0.25 : (hovered ? 0.12 : 0))
                        readonly property color fillColor: Qt.tint(root.panelColor,
                            Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, highlightOpacity))
                        readonly property color foregroundColor: root.darkTheme
                            ? root.textColor : root.readableTextColor(fillColor)

                        objectName: modelData.objectName
                        text: modelData.label
                        checkable: false
                        checked: root.toolModel.activeShape === modelData.shape
                        enabled: root.toolModel.enabled

                        Layout.fillWidth: true
                        Layout.preferredHeight: 34

                        leftPadding: 8
                        rightPadding: 8
                        topPadding: 3
                        bottomPadding: 3

                        background: Rectangle {
                            radius: 3
                            color: shapeButton.fillColor
                            border.width: shapeButton.activeFocus ? 2 : 0
                            border.color: shapeButton.foregroundColor
                        }

                        contentItem: RowLayout {
                            spacing: 8

                            // PaintedEngravingItem currently computes its paint rect
                            // from parentItem(). Keep that parent at icon size:
                            // if it is the entire RowLayout, glyphs are painted outside
                            // the 48px preview strip and appear to disappear.
                            Item {
                                Layout.preferredWidth: 48
                                Layout.preferredHeight: 28
                                Layout.alignment: Qt.AlignVCenter
                                clip: true

                                PaintedEngravingItem {
                                    anchors.fill: parent
                                    engravingItem: nativePreviewModel.previewElement(shapeButton.modelData.shape)
                                    previewColor: shapeButton.foregroundColor
                                    numStaffLines: 0
                                    spatium: 5.5
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                text: shapeButton.modelData.label
                                color: shapeButton.foregroundColor
                                horizontalAlignment: Text.AlignLeft
                                verticalAlignment: Text.AlignVCenter
                                maximumLineCount: 1
                                elide: Text.ElideRight
                            }
                        }

                        onClicked: root.chooseShape(modelData.shape)
                    }
                }
            }
        }

        Label {
            color: root.textColor
            text: root.toolModel.activeTool === RepriseToolModel.Measure
                  ? qsTr("Double-click a measure to open its properties.")
                  : root.toolModel.activeTool === RepriseToolModel.Repeat
                  ? qsTr("Double-click a measure to manage repeats.")
                  : root.toolModel.activeTool === RepriseToolModel.SmartShape
                  ? (root.toolModel.operationActive
                     ? (root.noteAnchoredShape(root.toolModel.activeShape)
                        ? qsTr("Circles: captured notes. X: no valid destination note. Esc cancels")
                        : qsTr("Preview follows musical time; release at the desired position. Esc cancels"))
                     : (root.noteAnchoredShape(root.toolModel.activeShape)
                        ? qsTr("Double-click and hold, then drag to a destination note. Esc: Selection")
                        : qsTr("Double-click and hold, then drag forward in musical time. Esc: Selection")))
                  : (root.toolModel.activeTool === RepriseToolModel.Articulation
                     ? qsTr("Double-click a note or Shift-drag around several notes to choose an articulation.")
                     : (root.toolModel.activeTool === RepriseToolModel.TimeSignature
                        ? qsTr("Double-click a measure to choose a time signature.")
                        : (root.toolModel.activeTool === RepriseToolModel.Clef
                           ? qsTr("Double-click a measure on the desired staff to choose a clef.")
                           : (root.toolModel.activeTool === RepriseToolModel.KeySignature
                              ? qsTr("Double-click a measure to choose a key signature.")
                              : (root.toolModel.activeTool === RepriseToolModel.Expression
                                 ? qsTr("Double-click a note or rest to choose an expression.")
                                 : qsTr("Click to select. Middle mouse button: move the score."))))))
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }
}
