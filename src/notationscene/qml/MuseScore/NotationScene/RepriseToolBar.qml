/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Reprise Notation contributors
 */
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes
import Muse.GraphicalEffects
import Muse.Ui
import Muse.UiComponents
import MuseScore.NotationScene

FocusScope {
    id: root

    required property RepriseToolModel toolModel
    property NavigationSection navigationSection
    property bool noteInputActive: false
    signal toolRequested(int tool)
    signal noteInputRequested()
    property bool speedyEntryActive: false
    signal speedyEntryRequested()
    signal scoreFocusRequested()
    property bool showOnlyActiveVoice: false
    signal showOnlyActiveVoiceRequested(bool enabled)
    // The dock can shrink while the strip keeps every tool reachable.
    implicitWidth: 240
    implicitHeight: 64
    // Keep SVG paths editable and recolor them to match the active MuseScore theme.
    component RepriseSvgIcon: Item {
        required property url svgSource
        property color tintColor: ui.theme.fontPrimaryColor
        implicitWidth: 28
        implicitHeight: 28

        Image {
            id: svgTexture
            anchors.fill: parent
            source: svgSource
            sourceSize.width: 32
            sourceSize.height: 32
            fillMode: Image.PreserveAspectFit
            visible: false
        }

        EffectColorOverlay {
            anchors.fill: parent
            source: svgTexture
            color: tintColor
        }
    }

    // Reuse native button behavior and the existing theme-aware SVG renderer.
    component RepriseSvgButton: FlatButton {
        id: tile
        required property url svgSource
        Layout.preferredWidth: 68
        Layout.preferredHeight: 62
        transparent: !accentButton
        // Text remains the accessible label/tooltip; caption controls the
        // graphic, allowing predictable two-line labels for multiword tools.
        property string caption: text

        contentItem: ColumnLayout {
            spacing: 2
            RepriseSvgIcon {
                svgSource: tile.svgSource
                tintColor: tile.iconColor
                Layout.preferredWidth: 26
                Layout.preferredHeight: 26
                Layout.alignment: Qt.AlignHCenter
            }
            StyledTextLabel {
                text: tile.caption
                font.family: tile.textFont.family
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
                Layout.preferredWidth: Math.max(56, tile.width - 6)
                Layout.preferredHeight: 30
                Layout.alignment: Qt.AlignHCenter
                wrapMode: Text.WordWrap
                verticalAlignment: Text.AlignVCenter
                maximumLineCount: 2
                elide: Text.ElideRight
            }
        }
    }

    NavigationPanel {
        id: toolNavigation
        name: "RepriseTools"
        section: root.navigationSection
        order: 0
        direction: NavigationPanel.Horizontal
        enabled: root.enabled && root.visible
        accessible.name: qsTr("Tools")
    }

    // Narrow and restored windows can pan this strip without losing tools.
    Flickable {
        id: toolStrip
        anchors.fill: parent
        clip: true
        flickableDirection: Flickable.HorizontalFlick
        interactive: contentWidth > width
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: buttonRow.implicitWidth + 16
        contentHeight: height
        onWidthChanged: returnToBounds()

        RowLayout {
            id: buttonRow
            x: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 3

        RepriseSvgButton {
            id: selectionButton
            objectName: "repriseSelectionTool"
            svgSource: Qt.resolvedUrl("resources/reprise/select.svg")
            text: qsTr("Selection")
            Layout.preferredWidth: 68
            Layout.preferredHeight: 62
            
            toolTipTitle: qsTr("Selection")
            enabled: root.toolModel.enabled
            accentButton: !root.noteInputActive && root.toolModel.activeTool === RepriseToolModel.Selection
            navigation.panel: toolNavigation
            navigation.order: 1
            accessible.checked: !root.noteInputActive && root.toolModel.activeTool === RepriseToolModel.Selection
            onClicked: {
                root.toolRequested(RepriseToolModel.Selection)
                root.scoreFocusRequested()
            }
        }

        Rectangle {
            Layout.preferredWidth: 1
            Layout.preferredHeight: 24
            color: palette.mid
            readonly property SystemPalette palette: SystemPalette {}
        }

        RepriseSvgButton {
            objectName: "repriseSimpleEntryTool"
            text: qsTr("Simple Entry")
            caption: qsTr("Simple\nEntry")
            svgSource: Qt.resolvedUrl("resources/reprise/simple_entry.svg")
            toolTipTitle: text
            enabled: root.toolModel.enabled
            accentButton: root.noteInputActive && !root.speedyEntryActive
            accessible.checked: accentButton
            navigation.panel: toolNavigation
            navigation.order: 2
            onClicked: root.noteInputRequested()
        }

        Rectangle {
            Layout.preferredWidth: 1
            Layout.preferredHeight: 24
            color: palette.mid
            readonly property SystemPalette palette: SystemPalette {}
        }

        RepriseSvgButton {
            objectName: "repriseSpeedyEntryTool"
            text: qsTr("Speedy Entry")
            svgSource: Qt.resolvedUrl("resources/reprise/speedy_entry.svg")
            toolTipTitle: text
            toolTipDescription: qsTr("Choose a pitch, then press a number to enter the note")
            enabled: root.toolModel.enabled
            accentButton: root.speedyEntryActive
            accessible.checked: accentButton
            navigation.panel: toolNavigation
            navigation.order: 3
            onClicked: root.speedyEntryRequested()
        }

        FlatButton {
            accessible.name: qsTr("Speedy Entry options")
            icon: IconCode.SMALL_ARROW_DOWN
            visible: root.speedyEntryActive
            navigation.panel: toolNavigation
            navigation.order: 4
            onClicked: speedyMenu.toggleOpened([
                { id: "speedy-midi", title: qsTr("Use MIDI Device for Input"), enabled: false }
            ])

            StyledMenuLoader {
                id: speedyMenu
                menuAnchorItem: ui.rootItem
            }
        }

        RepriseSvgButton {
            objectName: "repriseKeySignatureTool"
            svgSource: Qt.resolvedUrl("resources/reprise/key_signature.svg")
            text: qsTr("Key Signature")
            caption: qsTr("Key\nSignature")
            Layout.preferredWidth: 68
            Layout.preferredHeight: 62

            toolTipTitle: qsTr("Key Signature")
            toolTipDescription: qsTr("Double-click a measure to choose a key signature")
            enabled: root.toolModel.enabled
            accentButton: root.toolModel.activeTool === RepriseToolModel.KeySignature
            navigation.panel: toolNavigation
            navigation.order: 5
            accessible.checked: root.toolModel.activeTool === RepriseToolModel.KeySignature
            onClicked: {
                root.toolRequested(RepriseToolModel.KeySignature)
                root.scoreFocusRequested()
            }
        }

        RepriseSvgButton {
            id: timeSignatureButton
            objectName: "repriseTimeSignatureTool"
            svgSource: Qt.resolvedUrl("resources/reprise/time_signature.svg")
            text: qsTr("Time Signature")
            caption: qsTr("Time\nSignature")
            Layout.preferredWidth: 68
            Layout.preferredHeight: 62
            
            toolTipTitle: qsTr("Time Signature")
            toolTipDescription: qsTr("Double-click a measure to choose a time signature")
            enabled: root.toolModel.enabled
            accentButton: root.toolModel.activeTool === RepriseToolModel.TimeSignature
            navigation.panel: toolNavigation
            navigation.order: 6
            accessible.checked: root.toolModel.activeTool === RepriseToolModel.TimeSignature
            onClicked: {
                root.toolRequested(RepriseToolModel.TimeSignature)
                root.scoreFocusRequested()
            }
        }

        RepriseSvgButton {
            id: clefButton
            objectName: "repriseClefTool"
            svgSource: Qt.resolvedUrl("resources/reprise/change_clef.svg")
            text: qsTr("Change Clef")
            caption: qsTr("Change\nClef")
            Layout.preferredWidth: 68
            Layout.preferredHeight: 62
            
            toolTipTitle: qsTr("Change Clef")
            toolTipDescription: qsTr("Double-click a measure to choose a clef")
            enabled: root.toolModel.enabled
            accentButton: root.toolModel.activeTool === RepriseToolModel.Clef
            navigation.panel: toolNavigation
            navigation.order: 7
            accessible.checked: root.toolModel.activeTool === RepriseToolModel.Clef
            onClicked: {
                root.toolRequested(RepriseToolModel.Clef)
                root.scoreFocusRequested()
            }
        }

        RepriseSvgButton {
            objectName: "repriseMeasureTool"
            text: qsTr("Measure")
            svgSource: Qt.resolvedUrl("resources/reprise/measure.svg")
            toolTipTitle: text
            toolTipDescription: qsTr("Double-click a measure to open its properties")
            enabled: root.toolModel.enabled
            accentButton: root.toolModel.activeTool === RepriseToolModel.Measure
            accessible.checked: accentButton
            navigation.panel: toolNavigation
            navigation.order: 8
            onClicked: {
                root.toolRequested(RepriseToolModel.Measure)
                root.scoreFocusRequested()
            }
        }

        Rectangle {
            Layout.preferredWidth: 1
            Layout.preferredHeight: 24
            color: palette.mid
            readonly property SystemPalette palette: SystemPalette {}
        }

        RepriseSvgButton {
            id: smartShapeButton
            objectName: "repriseSmartShapeTool"
            svgSource: Qt.resolvedUrl("resources/reprise/smart_shape.svg")
            text: qsTr("Smart Shape")
            caption: qsTr("Smart\nShape")
            Layout.preferredWidth: 68
            Layout.preferredHeight: 62
            
            toolTipTitle: qsTr("Smart Shape")
            enabled: root.toolModel.enabled
            accentButton: root.toolModel.activeTool === RepriseToolModel.SmartShape
            navigation.panel: toolNavigation
            navigation.order: 9
            accessible.checked: root.toolModel.activeTool === RepriseToolModel.SmartShape
            onClicked: {
                root.toolRequested(RepriseToolModel.SmartShape)
                root.scoreFocusRequested()
            }
        }

        RepriseSvgButton {
            id: articulationButton
            objectName: "repriseArticulationTool"
            svgSource: Qt.resolvedUrl("resources/reprise/articulation.svg")
            text: qsTr("Articulation")
            caption: qsTr("Articulation", "toolbar caption")
            Layout.preferredWidth: 68
            Layout.preferredHeight: 62
            
            toolTipTitle: qsTr("Articulation")
            toolTipDescription: qsTr("Double-click a note or Shift-drag around several notes to choose an articulation")
            enabled: root.toolModel.enabled
            accentButton: root.toolModel.activeTool === RepriseToolModel.Articulation
            navigation.panel: toolNavigation
            navigation.order: 10
            accessible.checked: root.toolModel.activeTool === RepriseToolModel.Articulation
            onClicked: {
                root.toolRequested(RepriseToolModel.Articulation)
                root.scoreFocusRequested()
            }
        }

        RepriseSvgButton {
            objectName: "repriseExpressionTool"
            svgSource: Qt.resolvedUrl("resources/reprise/expression.svg")
            text: qsTr("Expression")
            Layout.preferredWidth: 68
            Layout.preferredHeight: 62

            toolTipTitle: qsTr("Expression")
            toolTipDescription: qsTr("Double-click a note or rest to choose a native expression")
            enabled: root.toolModel.enabled
            accentButton: root.toolModel.activeTool === RepriseToolModel.Expression
            navigation.panel: toolNavigation
            navigation.order: 11
            accessible.checked: root.toolModel.activeTool === RepriseToolModel.Expression
            onClicked: {
                root.toolRequested(RepriseToolModel.Expression)
                root.scoreFocusRequested()
            }
        }

        RepriseSvgButton {
            objectName: "repriseRepeatTool"
            svgSource: Qt.resolvedUrl("resources/reprise/repeat_tool.svg")
            text: qsTr("Repeat")
            enabled: root.toolModel.enabled
            navigation.panel: toolNavigation
            navigation.order: 12
            accentButton: root.toolModel.activeTool === RepriseToolModel.Repeat
            accessible.checked: accentButton
            toolTipDescription: qsTr("Double-click a measure to manage repeats")
            onClicked: {
                root.toolRequested(RepriseToolModel.Repeat)
                root.scoreFocusRequested()
            }
        }
        RepriseSvgButton {
            objectName: "repriseActiveVoiceOnly"
            text: qsTr("Active Voice Only")
            caption: qsTr("Active Voice\nOnly")
            svgSource: Qt.resolvedUrl("resources/reprise/active_voice.svg")
            toolTipTitle: qsTr("Show active voice only")
            accessible.checked: root.showOnlyActiveVoice
            accentButton: root.showOnlyActiveVoice
            enabled: root.toolModel.enabled
            navigation.panel: toolNavigation
            navigation.order: 13
            onClicked: {
                root.showOnlyActiveVoiceRequested(!root.showOnlyActiveVoice)
                root.scoreFocusRequested()
            }
        }


    }
    }

    // Explicit scrolling affordances for compact windows: later tools never
    // silently disappear when the user restores from maximized.
    FlatButton {
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: 22
        height: 42
        visible: toolStrip.contentWidth > toolStrip.width && toolStrip.contentX > 1
        icon: IconCode.CHEVRON_LEFT
        transparent: false
        toolTipTitle: qsTr("Previous Reprise tools")
        onClicked: toolStrip.contentX = Math.max(0, toolStrip.contentX - 260)
    }

    FlatButton {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: 22
        height: 42
        visible: toolStrip.contentWidth > toolStrip.width
                 && toolStrip.contentX < toolStrip.contentWidth - toolStrip.width - 1
        icon: IconCode.CHEVRON_RIGHT
        transparent: false
        toolTipTitle: qsTr("Next Reprise tools")
        onClicked: toolStrip.contentX = Math.min(toolStrip.contentWidth - toolStrip.width,
                                                toolStrip.contentX + 260)
    }
}
