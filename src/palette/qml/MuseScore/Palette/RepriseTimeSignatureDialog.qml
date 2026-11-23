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

    objectName: "repriseTimeSignatureSelectionDialog"
    title: qsTr("Time Signature Selection")
    contentWidth: 620
    contentHeight: 430
    margins: 12

    property int selectedRow: -1

    function showForCurrentSelection() {
        selectedRow = -1
        timeSignatureGrid.currentIndex = -1
        show()
    }

    function applySelected() {
        if (selectedRow < 0 || !timeSignatureModel.available) {
            return
        }
        if (timeSignatureModel.apply(selectedRow)) {
            hide()
        }
    }

    function openCreator() {
        if (!timeSignatureModel.creatorAvailable) {
            return
        }
        hide()
        Qt.callLater(timeSignatureModel.openCreator)
    }

    RepriseTimeSignatureHelper {
        id: timeSignatureModel
    }

    NavigationPanel {
        id: navigationPanel
        name: "RepriseTimeSignatureSelection"
        section: root.navigationSection
        enabled: root.isOpened
        order: 1
        direction: NavigationPanel.Both
    }

    onNavigationActivateRequested: timeSignatureGrid.forceActiveFocus()

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        StyledTextLabel {
            Layout.fillWidth: true
            text: timeSignatureModel.available
                  ? qsTr("Choose a time signature to apply to the selected measure.")
                  : qsTr("The Time Signatures catalog is not available.")
            wrapMode: Text.WordWrap
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: ui.theme.textFieldColor
            border.color: ui.theme.strokeColor
            radius: 2

            GridView {
                id: timeSignatureGrid
                anchors.fill: parent
                anchors.margins: 1
                clip: true
                cellWidth: 76
                cellHeight: 68
                keyNavigationEnabled: true
                currentIndex: -1
                model: timeSignatureModel

                onCurrentIndexChanged: {
                    if (currentIndex >= 0) {
                        root.selectedRow = currentIndex
                    }
                }

                delegate: ListItemBlank {
                    id: timeSignatureCell

                    required property var model
                    required property int index

                    width: timeSignatureGrid.cellWidth
                    height: timeSignatureGrid.cellHeight
                    isSelected: root.selectedRow === index
                    hint: model.toolTip

                    navigation.panel: navigationPanel
                    navigation.row: Math.floor(index / Math.max(1, Math.floor(timeSignatureGrid.width / timeSignatureGrid.cellWidth)))
                    navigation.column: index % Math.max(1, Math.floor(timeSignatureGrid.width / timeSignatureGrid.cellWidth))
                    navigation.accessible.name: model.accessibleText

                    IconView {
                        anchors.fill: parent
                        anchors.margins: 7
                        icon: timeSignatureCell.model.decoration
                    }

                    onClicked: {
                        root.selectedRow = index
                        timeSignatureGrid.currentIndex = index
                    }

                    onDoubleClicked: {
                        root.selectedRow = index
                        timeSignatureGrid.currentIndex = index
                        root.applySelected()
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            FlatButton {
                text: qsTrc("palette", "Create time signature")
                enabled: timeSignatureModel.creatorAvailable
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
                enabled: timeSignatureModel.available && root.selectedRow >= 0
                onClicked: root.applySelected()
            }
        }
    }
}
