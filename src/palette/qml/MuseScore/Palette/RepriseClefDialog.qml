/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Reprise Notation contributors
 */
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

import Muse.Ui
import Muse.UiComponents
import MuseScore.Palette

import "internal"

StyledDialogView {
    id: root

    objectName: "repriseClefSelectionDialog"
    title: qsTr("Change Clef")
    contentWidth: 620
    contentHeight: 430
    margins: 12

    property int selectedRow: -1

    function showForCurrentSelection() {
        selectedRow = -1
        clefGrid.currentIndex = -1
        show()
    }

    function applySelected() {
        if (selectedRow < 0 || !clefModel.available) {
            return
        }
        if (clefModel.apply(selectedRow)) {
            hide()
        }
    }

    RepriseClefHelper {
        id: clefModel
    }

    NavigationPanel {
        id: navigationPanel
        name: "RepriseClefSelection"
        section: root.navigationSection
        enabled: root.isOpened
        order: 1
        direction: NavigationPanel.Both
    }

    onNavigationActivateRequested: clefGrid.forceActiveFocus()

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        StyledTextLabel {
            Layout.fillWidth: true
            text: clefModel.available
                  ? qsTr("Choose a clef to apply to the selected measure and staff.")
                  : qsTr("The Clefs catalog is not available.")
            wrapMode: Text.WordWrap
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: ui.theme.textFieldColor
            border.color: ui.theme.strokeColor
            radius: 2

            GridView {
                id: clefGrid
                anchors.fill: parent
                anchors.margins: 1
                clip: true
                cellWidth: 76
                cellHeight: 86
                keyNavigationEnabled: true
                currentIndex: -1
                model: clefModel

                onCurrentIndexChanged: {
                    if (currentIndex >= 0) {
                        root.selectedRow = currentIndex
                    }
                }

                delegate: ListItemBlank {
                    id: clefCell

                    required property var model
                    required property int index

                    width: clefGrid.cellWidth
                    height: clefGrid.cellHeight
                    isSelected: root.selectedRow === index
                    hint: model.toolTip

                    navigation.panel: navigationPanel
                    navigation.row: Math.floor(index / Math.max(1, Math.floor(clefGrid.width / clefGrid.cellWidth)))
                    navigation.column: index % Math.max(1, Math.floor(clefGrid.width / clefGrid.cellWidth))
                    navigation.accessible.name: model.accessibleText

                    IconView {
                        anchors.fill: parent
                        anchors.margins: 7
                        icon: clefCell.model.decoration
                    }

                    onClicked: {
                        root.selectedRow = index
                        clefGrid.currentIndex = index
                    }

                    onDoubleClicked: {
                        root.selectedRow = index
                        clefGrid.currentIndex = index
                        root.applySelected()
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Item {
                Layout.fillWidth: true
            }

            FlatButton {
                text: qsTr("Cancel")
                onClicked: root.hide()
            }

            FlatButton {
                text: qsTr("Select")
                accentButton: true
                enabled: clefModel.available && root.selectedRow >= 0
                onClicked: root.applySelected()
            }
        }
    }
}
