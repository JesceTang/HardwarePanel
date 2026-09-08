import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQml
import HWPanel.Charts 1.0

ApplicationWindow {
    id: root
    visible: true
    width: 1280
    height: 800
    minimumWidth: 1024
    minimumHeight: 640
    title: qsTr("HWPanel")
    color: "#14161a"

    readonly property color panelColor: "#1d2026"
    readonly property color textColor: "#e6e8ec"
    readonly property color mutedColor: "#8b93a1"

    header: Rectangle {
        color: root.panelColor
        height: 52
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            Label {
                text: "HWPanel"
                color: root.textColor
                font.pixelSize: 20
                font.bold: true
            }
            Item { Layout.fillWidth: true }
            Label {
                text: profiles.count > 0
                      ? qsTr("Profile: ") + profiles.activeId
                      : qsTr("No profiles")
                color: root.mutedColor
                font.pixelSize: 13
            }
            Rectangle {
                width: 10; height: 10; radius: 5
                color: connection.color
            }
            Label {
                text: connection.text
                color: root.mutedColor
                font.pixelSize: 13
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        // ---- Chart grid: 60 s realtime curves -----------------------------
        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: 2
            rows: 3
            columnSpacing: 12
            rowSpacing: 12

            Repeater {
                model: [
                    { title: qsTr("CPU Usage"),   unit: "%",    series: telemetry.cpuUsage,   color: "#4da3ff", auto: false, min: 0, max: 100 },
                    { title: qsTr("GPU Usage"),   unit: "%",    series: telemetry.gpuUsage,   color: "#3ddc84", auto: false, min: 0, max: 100 },
                    { title: qsTr("Memory Used"), unit: "MB",   series: telemetry.memUsedMb,  color: "#c792ea", auto: true,  min: 0, max: 100 },
                    { title: qsTr("Disk I/O"),    unit: "MB/s", series: telemetry.diskIoBps,  color: "#f0a030", auto: true,  min: 0, max: 100, scale: 1 / 1048576 },
                    { title: qsTr("CPU Temp"),    unit: "°C",   series: telemetry.cpuTempC,   color: "#e5484d", auto: true,  min: 0, max: 100 },
                    { title: qsTr("GPU Temp"),    unit: "°C",   series: telemetry.gpuTempC,   color: "#ff7b72", auto: true,  min: 0, max: 100 }
                ]
                delegate: Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: root.panelColor
                    radius: 8
                    required property var modelData

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                text: modelData.title
                                color: root.textColor
                                font.pixelSize: 14
                                font.bold: true
                            }
                            Item { Layout.fillWidth: true }
                            Label {
                                text: (modelData.scale !== undefined
                                       ? (modelData.series.latest * modelData.scale)
                                       : modelData.series.latest).toFixed(1)
                                      + " " + modelData.unit
                                color: modelData.color
                                font.pixelSize: 16
                                font.bold: true
                            }
                        }

                        LineChart {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            values: modelData.scale !== undefined
                                    ? modelData.series.values.map(function (v) { return v * modelData.scale; })
                                    : modelData.series.values
                            lineColor: modelData.color
                            autoRange: modelData.auto
                            minValue: modelData.min
                            maxValue: modelData.max
                        }
                    }
                }
            }
        }

        // ---- Right column: profiles + notifications -----------------------
        ColumnLayout {
            Layout.preferredWidth: 320
            Layout.fillHeight: true
            spacing: 12

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 280
                color: root.panelColor
                radius: 8

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 6

                    Label {
                        text: qsTr("Configuration Profiles")
                        color: root.textColor
                        font.pixelSize: 15
                        font.bold: true
                    }

                    ListView {
                        id: profileList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 6
                        model: profiles

                        delegate: Rectangle {
                            width: profileList.width
                            height: 64
                            radius: 6
                            color: isActive ? "#263140" : "#22262d"
                            border.color: isActive ? "#4da3ff" : "transparent"
                            border.width: 1

                            required property string profileId
                            required property string profileName
                            required property string profileDescription
                            required property bool isActive

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 2
                                Label {
                                    text: profileName + (isActive ? "  ●" : "")
                                    color: root.textColor
                                    font.pixelSize: 14
                                    font.bold: true
                                }
                                Label {
                                    text: profileDescription
                                    color: root.mutedColor
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    worker.SwitchProfile(profileId)
                                    worker.RefreshProfiles()
                                }
                            }
                        }
                    }

                    Button {
                        Layout.fillWidth: true
                        text: qsTr("Refresh")
                        onClicked: worker.RefreshProfiles()
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: root.panelColor
                radius: 8

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            text: qsTr("Notifications") + " (" + notifications.count + ")"
                            color: root.textColor
                            font.pixelSize: 15
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        Button {
                            text: qsTr("Clear")
                            flat: true
                            onClicked: notifications.clear()
                        }
                    }

                    ListView {
                        id: notificationList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 4
                        model: notifications

                        delegate: Rectangle {
                            width: notificationList.width
                            height: notifColumn.height + 12
                            radius: 6
                            color: "#22262d"

                            required property string time
                            required property string level
                            required property string title
                            required property string message

                            Column {
                                id: notifColumn
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.margins: 8
                                spacing: 2

                                Text {
                                    text: time + "  [" + level + "]  " + title
                                    color: level === "warn" || level === "error"
                                           ? "#f0a030" : root.mutedColor
                                    font.pixelSize: 11
                                }
                                Text {
                                    text: message
                                    color: root.textColor
                                    font.pixelSize: 13
                                    width: parent.width
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Component.onCompleted: worker.RefreshProfiles()
}
