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

FocusScope {
    id: root

    property NavigationSection navigationSection
    property int navigationOrderStart: 90
    signal closeRequested()
    implicitWidth: 280
    implicitHeight: 260

    function activate() {
        headerNavigation.requestActive()
    }

    function handleNavigationEvent(event) {
        if (event.type === NavigationEvent.Escape) {
            event.accepted = true
            Qt.callLater(root.closeRequested)
        }
    }

    Rectangle {
        anchors.fill: parent
        color: palette.window
        // Match the borderless Reprise tool context.
        border.width: 0
        readonly property SystemPalette palette: SystemPalette {}
    }

    RepriseCollectionsModel { id: collectionsModel }

    NavigationPanel {
        id: headerNavigation
        name: "RepriseCollectionsHeader"
        section: root.navigationSection
        order: root.navigationOrderStart
        direction: NavigationPanel.Horizontal
        enabled: root.enabled && root.visible
        accessible.name: qsTr("My collections")
        onNavigationEvent: function(event) {
            root.handleNavigationEvent(event)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            StyledTextLabel {
                text: qsTr("My collections")
                font.bold: true
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            FlatButton {
                objectName: "repriseCloseCollections"
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                icon: IconCode.CLOSE_X_ROUNDED
                toolTipTitle: qsTr("Back to tools")
                navigation.panel: headerNavigation
                navigation.order: 1
                onClicked: root.closeRequested()
            }
        }

        StyledTextLabel {
            text: qsTr("Collections are unavailable")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            visible: !collectionsModel.available
        }

        PaletteTree {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            enabled: collectionsModel.available
            visible: collectionsModel.available
            paletteProvider: collectionsModel.provider
            paletteModel: collectionsModel.paletteModel
            paletteController: collectionsModel.paletteController
            syncProviderSearch: false
            searchOpened: false
            filter: ""
            navigation.section: root.navigationSection
            navigation.order: root.navigationOrderStart + 1
            navigation.onNavigationEvent: function(event) {
                root.handleNavigationEvent(event)
            }
        }
    }
}
