/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Reprise Notation contributors
 */

import QtQuick
import QtQuick.Layouts
import Muse.Ui
import Muse.UiComponents

StyledDialogView {
    id: root

    title: qsTrc("appshell/feedback", "Feedback · Reprise Notation")
    contentWidth: 400
    contentHeight: content.implicitHeight + 32

    onOpened: copied.visible = false
    onNavigationActivateRequested: copyButton.navigation.requestActive()

    NavigationPanel {
        id: controlsNavigation
        name: "RepriseFeedbackControls"
        section: root.navigationSection
        order: 1
        direction: NavigationPanel.Horizontal
    }

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        StyledTextLabel {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignLeft
            wrapMode: Text.WordWrap
            text: qsTrc("appshell/feedback", "Report a problem or suggest a change by email. Include your Reprise Notation version if you can.")
        }

        RowLayout {
            Layout.fillWidth: true

            TextInputField {
                id: address
                Layout.fillWidth: true
                currentText: "info@emanuelecampilongo.com"
                readOnly: true
                accessible.name: qsTrc("appshell/feedback", "Email address")
                navigation.panel: controlsNavigation
                navigation.order: 1
            }

            FlatButton {
                id: copyButton
                icon: IconCode.COPY
                toolTipTitle: qsTrc("appshell/feedback", "Copy email address")
                accessible.name: toolTipTitle
                navigation.panel: controlsNavigation
                navigation.order: 2

                onClicked: {
                    address.selectAll()
                    address.inputField.copy()
                    address.inputField.deselect()
                    copied.visible = true
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true

            StyledTextLabel {
                id: copied
                visible: false
                text: qsTrc("appshell/feedback", "Address copied")
            }

            Item { Layout.fillWidth: true }

            FlatButton {
                text: qsTrc("appshell/feedback", "Close")
                navigation.panel: controlsNavigation
                navigation.order: 3
                onClicked: root.hide()
            }
        }
    }
}
