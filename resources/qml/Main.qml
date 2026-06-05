import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
    id: root
    visible: true
    title: "文搜搜"
    width: 1280
    height: 820
    minimumWidth: 980
    minimumHeight: 680
    color: "#f3f6fb"

    property string query: ""
    property int selectedRootIndex: 0
    property int selectedDateIndex: 0
    property var selectedExtensions: []
    property var dateValues: [0, 1, 7, 30, 365]

    Component.onCompleted: {
        selectedExtensions = controller.enabledExtensions.slice(0)
    }

    function rootId() {
        return selectedRootIndex <= 0 ? 0 : controller.rootsModel.idAt(selectedRootIndex - 1)
    }

    function runSearch() {
        controller.search(searchField.text, rootId(), selectedExtensions, dateValues[selectedDateIndex])
    }

    function toggleExtension(ext) {
        var copy = selectedExtensions.slice(0)
        var i = copy.indexOf(ext)
        if (i >= 0) {
            if (copy.length === 1) {
                toast.text = "至少保留一种文件类型"
                toast.open()
                return
            }
            copy.splice(i, 1)
        } else {
            copy.push(ext)
        }
        selectedExtensions = copy
        runSearch()
    }

    FontLoader {
        id: uiFont
        source: ""
    }

    Rectangle {
        anchors.fill: parent
        color: "#f3f6fb"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 28
            spacing: 18

            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                Rectangle {
                    width: 52
                    height: 52
                    radius: 14
                    gradient: Gradient {
                        GradientStop { position: 0; color: "#0f766e" }
                        GradientStop { position: 1; color: "#14b8a6" }
                    }
                    Text {
                        anchors.centerIn: parent
                        text: "文"
                        color: "white"
                        font.pixelSize: 27
                        font.bold: true
                    }
                }

                ColumnLayout {
                    spacing: 2
                    Text {
                        text: "文搜搜"
                        color: "#172033"
                        font.pixelSize: 30
                        font.bold: true
                    }
                    Text {
                        text: "离线文档全文检索工作台"
                        color: "#64748b"
                        font.pixelSize: 14
                    }
                }

                Item { Layout.fillWidth: true }

                StatPill { title: "目录"; value: controller.rootNames.length }
                StatPill { title: "索引"; value: controller.indexRunning ? "更新中" : "空闲" }
                StatPill { title: "状态"; value: controller.resultSummary }
            }

            Card {
                Layout.fillWidth: true
                Layout.preferredHeight: 206
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 16

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "搜索本机文档"
                            color: "#0f172a"
                            font.pixelSize: 24
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        SecondaryButton {
                            text: "添加目录"
                            onClicked: controller.addRoot()
                        }
                        SecondaryButton {
                            text: "全部更新"
                            onClicked: controller.updateAllRoots()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        TextField {
                            id: searchField
                            Layout.fillWidth: true
                            placeholderText: "输入关键词进行全文搜索"
                            selectByMouse: true
                            font.pixelSize: 19
                            leftPadding: 18
                            rightPadding: 18
                            topPadding: 13
                            bottomPadding: 13
                            background: Rectangle {
                                radius: 14
                                color: "white"
                                border.color: searchField.activeFocus ? "#0f766e" : "#b7eee8"
                                border.width: searchField.activeFocus ? 2 : 1
                            }
                            onAccepted: root.runSearch()
                        }
                        PrimaryButton {
                            text: "搜索"
                            implicitHeight: 54
                            onClicked: root.runSearch()
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 58
                        radius: 13
                        color: "#f8fafc"
                        border.color: "#edf2f7"
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14
                            spacing: 10
                            Text {
                                text: "筛选"
                                color: "#64748b"
                                font.pixelSize: 13
                                font.bold: true
                            }
                            ComboBox {
                                id: rootCombo
                                model: ["全部目录"].concat(controller.rootNames)
                                currentIndex: selectedRootIndex
                                onActivated: {
                                    selectedRootIndex = currentIndex
                                    root.runSearch()
                                }
                            }
                            ComboBox {
                                id: dateCombo
                                model: ["全部日期", "最近一天", "最近 7 天", "最近 30 天", "最近一年"]
                                currentIndex: selectedDateIndex
                                onActivated: {
                                    selectedDateIndex = currentIndex
                                    root.runSearch()
                                }
                            }
                            Flow {
                                Layout.fillWidth: true
                                spacing: 8
                                Repeater {
                                    model: controller.enabledExtensions
                                    delegate: FilterChip {
                                        text: modelData
                                        checked: selectedExtensions.indexOf(modelData) >= 0
                                        onClicked: root.toggleExtension(modelData)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: "搜索结果"
                    color: "#64748b"
                    font.pixelSize: 13
                    font.bold: true
                }
                Text {
                    text: controller.resultSummary
                    color: "#0f172a"
                    font.pixelSize: 22
                    font.bold: true
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: controller.statusText + (controller.progressText.length ? "  ·  " + controller.progressText : "")
                    color: "#64748b"
                    font.pixelSize: 14
                    elide: Text.ElideRight
                    Layout.maximumWidth: 520
                }
            }

            ListView {
                id: resultsView
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: controller.resultsModel
                spacing: 10
                clip: true
                delegate: ResultCard {
                    width: resultsView.width
                    titleHtml: filenameHtml
                    snippetHtml: snippetHtml
                    pathText: path
                    metaText: extension + " · " + sizeText + " · " + modifiedText
                    onPreview: {
                        previewDialog.titleText = filename
                        previewDialog.bodyHtml = controller.previewHtml(documentId, searchField.text)
                        previewDialog.open()
                    }
                    onOpenFile: controller.openDocument(path)
                    onOpenFolder: controller.openFolder(path)
                }

                ScrollBar.vertical: ScrollBar {}

                Text {
                    anchors.centerIn: parent
                    visible: resultsView.count === 0
                    text: "输入关键词后开始搜索"
                    color: "#94a3b8"
                    font.pixelSize: 22
                    font.bold: true
                }
            }
        }
    }

    Dialog {
        id: previewDialog
        modal: true
        width: Math.min(root.width - 120, 980)
        height: Math.min(root.height - 120, 720)
        x: (root.width - width) / 2
        y: (root.height - height) / 2
        property string titleText: ""
        property string bodyHtml: ""
        title: titleText
        standardButtons: Dialog.Close
        contentItem: ScrollView {
            TextArea {
                readOnly: true
                textFormat: TextEdit.RichText
                wrapMode: TextEdit.Wrap
                text: previewDialog.bodyHtml
                selectByMouse: true
                color: "#172033"
                font.pixelSize: 15
                background: Rectangle {
                    color: "#ffffff"
                    radius: 12
                    border.color: "#e6eaf0"
                }
            }
        }
    }

    Dialog {
        id: toast
        modal: false
        x: root.width - width - 34
        y: 34
        width: 260
        property alias text: toastText.text
        contentItem: Rectangle {
            radius: 12
            color: "#172033"
            implicitHeight: 54
            Text {
                id: toastText
                anchors.centerIn: parent
                color: "white"
                font.pixelSize: 14
            }
        }
    }

    component Card: Rectangle {
        color: "#ffffff"
        radius: 20
        border.color: "#e6eaf0"
    }

    component StatPill: Rectangle {
        property string title: ""
        property string value: ""
        implicitWidth: 118
        implicitHeight: 58
        radius: 14
        color: "#ffffff"
        border.color: "#e5eaf3"
        Column {
            anchors.centerIn: parent
            spacing: 2
            Text { text: title; color: "#64748b"; font.pixelSize: 12; anchors.horizontalCenter: parent.horizontalCenter }
            Text { text: value; color: "#172033"; font.pixelSize: 16; font.bold: true; anchors.horizontalCenter: parent.horizontalCenter; elide: Text.ElideRight; width: 98; horizontalAlignment: Text.AlignHCenter }
        }
    }

    component PrimaryButton: Button {
        id: primaryControl
        font.bold: true
        font.pixelSize: 16
        contentItem: Text {
            text: primaryControl.text
            color: "white"
            font: primaryControl.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 13
            gradient: Gradient {
                GradientStop { position: 0; color: primaryControl.down ? "#115e59" : "#0f766e" }
                GradientStop { position: 1; color: primaryControl.down ? "#115e59" : "#14b8a6" }
            }
        }
    }

    component SecondaryButton: Button {
        id: secondaryControl
        font.pixelSize: 14
        contentItem: Text {
            text: secondaryControl.text
            color: "#334155"
            font: secondaryControl.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 11
            color: secondaryControl.hovered ? "#f0fdfa" : "#ffffff"
            border.color: secondaryControl.hovered ? "#7dd3fc" : "#dbe4ee"
        }
    }

    component FilterChip: Button {
        id: chipControl
        font.pixelSize: 13
        implicitHeight: 32
        contentItem: Text {
            text: chipControl.text
            color: chipControl.checked ? "#0f766e" : "#475569"
            font: chipControl.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 16
            color: chipControl.checked ? "#ccfbf1" : "#ffffff"
            border.color: chipControl.checked ? "#99f6e4" : "#dbe4ee"
        }
    }

    component ResultCard: Rectangle {
        signal preview()
        signal openFile()
        signal openFolder()
        property string titleHtml: ""
        property string snippetHtml: ""
        property string pathText: ""
        property string metaText: ""
        height: 118
        radius: 16
        color: "#ffffff"
        border.color: "#e6eaf0"

        RowLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 14
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                Text {
                    Layout.fillWidth: true
                    text: titleHtml
                    textFormat: Text.RichText
                    color: "#172033"
                    font.pixelSize: 17
                    font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    text: snippetHtml
                    textFormat: Text.RichText
                    color: "#334155"
                    font.pixelSize: 15
                    elide: Text.ElideRight
                    maximumLineCount: 2
                    wrapMode: Text.Wrap
                }
                Text {
                    Layout.fillWidth: true
                    text: pathText + "  ·  " + metaText
                    color: "#94a3b8"
                    font.pixelSize: 12
                    elide: Text.ElideMiddle
                }
            }
            SecondaryButton { text: "预览"; onClicked: preview() }
            SecondaryButton { text: "打开"; onClicked: openFile() }
            SecondaryButton { text: "文件夹"; onClicked: openFolder() }
        }
    }
}
