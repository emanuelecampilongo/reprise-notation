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

    objectName: "repriseArticulationSelectionDialog"
    title: qsTr("Articulation Selection")
    contentWidth: 620
    contentHeight: 430
    margins: 12

    property int selectedRow: -1

    function showForCurrentSelection() {
        selectedRow = -1
        articulationGrid.currentIndex = -1
        show()
    }

    function applySelected() {
        if (selectedRow < 0 || !articulationCatalog.available) {
            return
        }
        if (articulationCatalog.apply(selectedRow)) {
            hide()
        }
    }

    RepriseArticulationCatalogModel {
        id: articulationCatalog
    }

    NavigationPanel {
        id: navigationPanel
        name: "RepriseArticulationSelection"
        section: root.navigationSection
        enabled: root.isOpened
        order: 1
        direction: NavigationPanel.Both
    }

    onNavigationActivateRequested: {
        articulationGrid.forceActiveFocus()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        StyledTextLabel {
            Layout.fillWidth: true
            text: articulationCatalog.available
                  ? qsTr("Choose an articulation to apply to the selected note or notes.")
                  : qsTr("The Articulation catalog is not available.")
            wrapMode: Text.WordWrap
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: ui.theme.textFieldColor
            border.color: ui.theme.strokeColor
            radius: 2

            GridView {
                id: articulationGrid
                anchors.fill: parent
                anchors.margins: 1
                clip: true
                cellWidth: 62
                cellHeight: 62
                keyNavigationEnabled: true
                currentIndex: -1
                model: articulationCatalog

                onCurrentIndexChanged: {
                    if (currentIndex >= 0) {
                        root.selectedRow = currentIndex
                    }
                }

                delegate: ListItemBlank {
                    id: articulationCell

                    required property var model
                    required property int index

                    width: articulationGrid.cellWidth
                    height: articulationGrid.cellHeight
                    isSelected: root.selectedRow === index
                    hint: model.toolTip

                    navigation.panel: navigationPanel
                    navigation.row: Math.floor(index / Math.max(1, Math.floor(articulationGrid.width / articulationGrid.cellWidth)))
                    navigation.column: index % Math.max(1, Math.floor(articulationGrid.width / articulationGrid.cellWidth))
                    navigation.accessible.name: model.accessibleText

                    IconView {
                        anchors.fill: parent
                        anchors.margins: 5
                        icon: articulationCell.model.decoration
                    }

                    onClicked: {
                        root.selectedRow = index
                        articulationGrid.currentIndex = index
                    }

                    onDoubleClicked: {
                        root.selectedRow = index
                        articulationGrid.currentIndex = index
                        root.applySelected()
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignRight
            spacing: 8

            FlatButton {
                text: qsTr("Cancel")
                onClicked: root.hide()
            }

            FlatButton {
                text: qsTr("Select")
                accentButton: true
                enabled: articulationCatalog.available && root.selectedRow >= 0
                onClicked: root.applySelected()
            }
        }
    }
}
