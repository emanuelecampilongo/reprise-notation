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

    objectName: "repriseKeySignatureSelectionDialog"
    title: qsTr("Key Signature Selection")
    contentWidth: 620
    contentHeight: 440
    margins: 12

    property int selectedRow: -1

    function showForCurrentSelection() {
        selectedRow = -1
        keySignatureGrid.currentIndex = -1
        transposeSelection.checked = false
        show()
    }

    function applySelected() {
        if (selectedRow < 0 || !keySignatureModel.available) {
            return
        }
        if (keySignatureModel.apply(selectedRow, transposeSelection.enabled && transposeSelection.checked)) {
            hide()
        }
    }

    function openCreator() {
        if (!keySignatureModel.creatorAvailable) {
            return
        }
        hide()
        Qt.callLater(keySignatureModel.openCreator)
    }

    RepriseKeySignatureHelper {
        id: keySignatureModel
    }

    NavigationPanel {
        id: navigationPanel
        name: "RepriseKeySignatureSelection"
        section: root.navigationSection
        enabled: root.isOpened
        order: 1
        direction: NavigationPanel.Both
    }

    onNavigationActivateRequested: keySignatureGrid.forceActiveFocus()

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        StyledTextLabel {
            Layout.fillWidth: true
            text: keySignatureModel.available
                  ? qsTr("Choose a key signature to apply to the selected measure.")
                  : qsTr("The Key Signatures catalog is not available.")
            wrapMode: Text.WordWrap
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: ui.theme.textFieldColor
            border.color: ui.theme.strokeColor
            radius: 2

            GridView {
                id: keySignatureGrid
                anchors.fill: parent
                anchors.margins: 1
                clip: true
                cellWidth: 90
                cellHeight: 85
                keyNavigationEnabled: true
                currentIndex: -1
                model: keySignatureModel

                onCurrentIndexChanged: {
                    if (currentIndex >= 0) {
                        root.selectedRow = currentIndex
                    }
                }

                delegate: ListItemBlank {
                    id: keySignatureCell

                    required property var model
                    required property int index

                    width: keySignatureGrid.cellWidth
                    height: keySignatureGrid.cellHeight
                    isSelected: root.selectedRow === index
                    hint: model.toolTip

                    navigation.panel: navigationPanel
                    navigation.row: Math.floor(index / Math.max(1, Math.floor(keySignatureGrid.width / keySignatureGrid.cellWidth)))
                    navigation.column: index % Math.max(1, Math.floor(keySignatureGrid.width / keySignatureGrid.cellWidth))
                    navigation.accessible.name: model.accessibleText

                    IconView {
                        anchors.fill: parent
                        anchors.margins: 7
                        icon: keySignatureCell.model.decoration
                    }

                    onClicked: {
                        root.selectedRow = index
                        keySignatureGrid.currentIndex = index
                    }

                    onDoubleClicked: {
                        root.selectedRow = index
                        keySignatureGrid.currentIndex = index
                        root.applySelected()
                    }
                }
            }
        }

        CheckBox {
            id: transposeSelection
            Layout.fillWidth: true
            text: qsTr("Transpose selected measures to the new key")
            enabled: keySignatureModel.canTransposeSelection(root.selectedRow)
            onClicked: checked = !checked
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            FlatButton {
                text: qsTrc("palette", "Create key signature")
                enabled: keySignatureModel.creatorAvailable
                onClicked: root.openCreator()
            }

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
                enabled: keySignatureModel.available && root.selectedRow >= 0
                onClicked: root.applySelected()
            }
        }
    }
}
