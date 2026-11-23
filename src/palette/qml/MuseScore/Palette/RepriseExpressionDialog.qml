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

    objectName: repeatMode ? "repriseRepeatSelectionDialog" : "repriseExpressionSelectionDialog"
    title: repeatMode ? qsTr("Repeat Selection") : qsTr("Expression Selection")
    contentWidth: 620
    contentHeight: 430
    margins: 12

    property bool repeatMode: false
    property bool repeatEnd: false
    signal measurePropertiesRequested()

    function showForCurrentSelection(hasEndRepeat) {
        repeatEnd = hasEndRepeat === true
        resetSelection()
        show()
    }

    function resetSelection() {
        expressionGrid.currentIndex = -1
        expressionGrid.positionViewAtBeginning()
    }

    function applySelected() {
        if (expressionGrid.currentIndex < 0 || !expressionCatalog.available) {
            return
        }
        if (expressionCatalog.apply(expressionGrid.currentIndex)) {
            hide()
        }
    }

    RepriseExpressionCatalogModel {
        id: expressionCatalog
        repeatMode: root.repeatMode
    }

    NavigationPanel {
        id: navigationPanel
        name: root.repeatMode ? "RepriseRepeatSelection" : "RepriseExpressionSelection"
        section: root.navigationSection
        enabled: root.isOpened
        order: 1
        direction: NavigationPanel.Both
    }

    onNavigationActivateRequested: {
        expressionGrid.forceActiveFocus()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        StyledTextLabel {
            Layout.fillWidth: true
            text: root.repeatMode
                  ? (expressionCatalog.available
                     ? qsTr("Choose a repeat barline, ending, or jump for the selected measure.")
                     : qsTr("The Repeats & jumps catalog is not available."))
                  : (expressionCatalog.available
                     ? qsTr("Choose a native dynamic, tempo indication, or expression for the selected note or rest.")
                     : qsTr("The Expression catalog is not available."))
            wrapMode: Text.WordWrap
        }

        StyledDropdown {
            id: categoryFilter

            visible: !root.repeatMode
            Layout.preferredWidth: 240

            navigation.name: "ExpressionCategory"
            navigation.panel: navigationPanel
            navigation.row: 0
            navigation.accessible.name: qsTr("Expression category") + ": " + currentText

            model: [
                { text: qsTr("Show all"), value: RepriseExpressionCatalogModel.All },
                { text: qsTr("Dynamics"), value: RepriseExpressionCatalogModel.Dynamics },
                { text: qsTr("Tempo marks"), value: RepriseExpressionCatalogModel.TempoMarks },
                { text: qsTr("Tempo changes"), value: RepriseExpressionCatalogModel.TempoChanges },
                { text: qsTr("Expressive text"), value: RepriseExpressionCatalogModel.ExpressiveText },
                { text: qsTr("Technique text"), value: RepriseExpressionCatalogModel.TechniqueText },
                { text: qsTr("Rehearsal marks"), value: RepriseExpressionCatalogModel.RehearsalMarks }
            ]

            currentIndex: expressionCatalog.category
            onActivated: function(index, value) {
                expressionCatalog.category = value
                root.resetSelection()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: ui.theme.textFieldColor
            border.color: ui.theme.strokeColor
            radius: 2

            GridView {
                id: expressionGrid
                anchors.fill: parent
                anchors.margins: 1
                clip: true
                cellWidth: 96
                cellHeight: 70
                keyNavigationEnabled: true
                currentIndex: -1
                model: expressionCatalog

                delegate: ListItemBlank {
                    id: expressionCell

                    required property var model
                    required property int index

                    width: expressionGrid.cellWidth
                    height: expressionGrid.cellHeight
                    isSelected: expressionGrid.currentIndex === index
                    hint: model.toolTip

                    navigation.panel: navigationPanel
                    navigation.row: (root.repeatMode ? 0 : 1) + Math.floor(index / Math.max(1, Math.floor(expressionGrid.width / expressionGrid.cellWidth)))
                    navigation.column: index % Math.max(1, Math.floor(expressionGrid.width / expressionGrid.cellWidth))
                    navigation.accessible.name: model.accessibleText

                    IconView {
                        anchors.fill: parent
                        anchors.margins: 5
                        icon: expressionCell.model.decoration
                    }

                    onClicked: {
                        expressionGrid.currentIndex = index
                    }

                    onDoubleClicked: {
                        expressionGrid.currentIndex = index
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
                visible: root.repeatMode && root.repeatEnd
                text: qsTr("Edit repeat count…")
                onClicked: {
                    root.hide()
                    Qt.callLater(root.measurePropertiesRequested)
                }
            }

            Item {
                visible: root.repeatMode
                Layout.fillWidth: true
            }

            FlatButton {
                text: qsTr("Cancel")
                onClicked: root.hide()
            }

            FlatButton {
                text: qsTr("Select")
                accentButton: true
                enabled: expressionCatalog.available && expressionGrid.currentIndex >= 0
                onClicked: root.applySelected()
            }
        }
    }
}
