import QtQuick 2.15

Item {
    id: root
    width: parent ? parent.width : 1280
    height: parent ? parent.height : 720
    property bool loginCaretVisible: true
    property real uiScale: Math.max(0.5, uiState.uiScale || 1.0)
    readonly property var uiTheme: uiState.uiTheme

    component UiRectangle: Rectangle {
        radius: Math.max(0, Math.min(uiState.uiTheme.cornerRadius, width / 2, height / 2))
    }

    component UiText: Text {
        font.family: uiState.uiTheme.uiFontFamily
    }

    function rgbaColor(red, green, blue, alpha) {
        return Qt.rgba(red / 255.0, green / 255.0, blue / 255.0, Math.max(0.0, Math.min(1.0, alpha)))
    }

    function chatChromeOpacity(chatUi) {
        return Math.max(0.2, Math.min(1.0, ((chatUi && chatUi.windowOpacityPercent) || 50) / 100.0))
    }

    function clampValue(value, minValue, maxValue) {
        if (maxValue < minValue) {
            return minValue
        }
        return Math.max(minValue, Math.min(maxValue, value))
    }

    function clampOverlayX(x, overlayWidth) {
        return clampValue(x, 0, root.width - overlayWidth)
    }

    function clampOverlayY(y, overlayHeight) {
        return clampValue(y, 0, root.height - overlayHeight)
    }

    function itemIconSource(itemId, identified) {
        if (!(itemId > 0)) {
            return ""
        }
        let source = "image://openmidgard/item/" + itemId
        if (identified !== undefined) {
            source += "?identified=" + (identified ? "1" : "0")
        }
        return source
    }

    function collectionImageSource(itemId, identified) {
        if (!(itemId > 0)) {
            return ""
        }
        let source = "image://openmidgard/collection/" + itemId
        if (identified !== undefined) {
            source += "?identified=" + (identified ? "1" : "0")
        }
        return source
    }

    function illustImageSource(itemId) {
        return itemId > 0 ? "image://openmidgard/illust/" + itemId : ""
    }

    function isHexDigit(ch) {
        return (ch >= "0" && ch <= "9")
            || (ch >= "a" && ch <= "f")
            || (ch >= "A" && ch <= "F")
    }

    function escapeHtml(text) {
        return String(text)
            .replace(/&/g, "&amp;")
            .replace(/</g, "&lt;")
            .replace(/>/g, "&gt;")
            .replace(/\"/g, "&quot;")
            .replace(/'/g, "&#39;")
    }

    function roColorTextToRichText(text) {
        if (!text) {
            return ""
        }

        let out = "<span style=\"color:#111111\">"
        let currentColor = "111111"
        for (let index = 0; index < text.length; ++index) {
            const ch = text[index]
            if (ch === "^" && index + 6 < text.length) {
                let isColor = true
                for (let colorIndex = 1; colorIndex <= 6; ++colorIndex) {
                    if (!isHexDigit(text[index + colorIndex])) {
                        isColor = false
                        break
                    }
                }
                if (isColor) {
                    currentColor = text.substring(index + 1, index + 7)
                    out += "</span><span style=\"color:#" + currentColor + "\">"
                    index += 6
                    continue
                }
            }

            if (ch === "\r") {
                continue
            }
            if (ch === "\n") {
                out += "<br>"
                continue
            }

            out += escapeHtml(ch === "_" ? " " : ch)
        }

        out += "</span>"
        return out
    }

    function skillIconSource(skillId) {
        return skillId > 0 ? "image://openmidgard/skill/" + skillId : ""
    }

    function statusIconSource(statusType) {
        return statusType >= 0 ? "image://openmidgard/status/" + statusType : ""
    }

    function formatStatusDuration(remainingMs) {
        var totalSeconds = Math.max(0, Math.ceil((remainingMs || 0) / 1000))
        var seconds = totalSeconds % 60
        var totalMinutes = Math.floor(totalSeconds / 60)
        var hours = Math.floor(totalMinutes / 60)
        var minutes = totalMinutes % 60

        if (hours > 0) {
            return hours + ":" + ("0" + minutes).slice(-2) + ":" + ("0" + seconds).slice(-2)
        }
        return totalMinutes + ":" + ("0" + seconds).slice(-2)
    }

    function statusTooltipText(statusData) {
        if (!statusData) {
            return ""
        }
        if (statusData.timed) {
            return (statusData.name || "") + "\n" + formatStatusDuration(statusData.remainingMs || 0) + " remaining"
        }
        return (statusData.name || "") + "\nActive"
    }

    function statusExpiryTintFraction(statusData) {
        if (!statusData || !statusData.timed) {
            return 0
        }
        var remainingMs = Math.max(0, statusData.remainingMs || 0)
        if (remainingMs >= 60000) {
            return 0
        }
        return 1 - (remainingMs / 60000)
    }

    function equipPreviewSource() {
        const revision = uiState.equipData.previewRevision || "0"
        return "image://openmidgard/equippreview?rev=" + revision
    }

    function makeCharPanelButtonsKey() {
        var buttons = uiState && uiState.makeCharButtons ? uiState.makeCharButtons : []
        var key = ""
        for (var i = 0; i < buttons.length; ++i) {
            key += (buttons[i].id || i) + ":" + (buttons[i].pressed ? "1" : "0") + ";"
        }
        return key
    }

    function charSelectPanelKey() {
        var details = uiState && uiState.charSelectSelectedDetails ? uiState.charSelectSelectedDetails : {}
        return details.imageRevision || "0"
    }

    Timer {
        interval: 500
        running: uiState.loginPanelVisible || uiState.makeCharVisible
        repeat: true
        onTriggered: root.loginCaretVisible = !root.loginCaretVisible
    }
    Image {
        anchors.fill: parent
        visible: source !== ""
        fillMode: Image.Stretch
        smooth: false
        cache: false
        source: (uiState.loginPanelVisible
            || uiState.serverSelectVisible
            || uiState.charSelectVisible
            || uiState.makeCharVisible
            || uiState.loadingVisible)
            ? "image://openmidgard/wallpaper?rev=" + encodeURIComponent(uiState.wallpaperRevision || "none")
            : ""
    }

    Repeater {
        model: uiState.anchors

        delegate: Item {
            id: worldAnchor
            required property var modelData
            property bool isPartyHpBar: modelData.kind === "partyHpBar"
            property bool isActorName: modelData.kind === "actorName"
            property bool isActorText: modelData.kind === "actorText"
            property int bubblePaddingX: modelData.showBubble === false ? 0 : (modelData.paddingX || 12)
            property int bubblePaddingY: modelData.showBubble === false ? 0 : (modelData.paddingY || 8)
            property int maxTextWidth: modelData.maxTextWidth || 0
            property bool wrapText: modelData.wrap === true
            property int textWidth: {
                const measured = Math.max(1, Math.ceil(label.contentWidth || label.implicitWidth || 1))
                return maxTextWidth > 0 ? Math.min(maxTextWidth, measured) : measured
            }
            property int anchorX: modelData.centerX !== undefined
                ? Math.round((modelData.centerX || 0) - width / 2)
                : (modelData.x || 0)
            property int anchorY: modelData.bottomY !== undefined
                ? Math.round((modelData.bottomY || 0) - height)
                : (modelData.y || 0)
            x: root.clampOverlayX(anchorX, width)
            y: root.clampOverlayY(anchorY, height)
            z: modelData.z || 2000
            width: isPartyHpBar ? (modelData.width || 60) : (textWidth + bubblePaddingX)
            height: isPartyHpBar ? (modelData.height || 5) : (Math.max(1, Math.ceil(label.implicitHeight)) + bubblePaddingY)

            UiRectangle {
                visible: parent.isPartyHpBar
                x: 0
                y: 0
                width: parent.width
                height: parent.height
                color: "#14199b"

                UiRectangle {
                    x: 1
                    y: 1
                    width: parent.width - 2
                    height: parent.height - 2
                    color: "#1e243a"
                }

                UiRectangle {
                    x: 1
                    y: 1
                    width: {
                        const maxHp = Math.max(1, modelData.maxHp || 0)
                        const currentHp = Math.max(0, Math.min(modelData.currentHp || 0, maxHp))
                        return Math.max(0, Math.floor((parent.width - 2) * currentHp / maxHp))
                    }
                    height: parent.height - 2
                    color: (modelData.maxHp > 0 && (modelData.currentHp || 0) * 4 < (modelData.maxHp || 0)) ? "#ea1616" : "#16ea25"
                }
            }

            UiRectangle {
                anchors.fill: parent
                radius: modelData.radius !== undefined ? modelData.radius : uiState.uiTheme.cornerRadius
                color: modelData.background
                visible: !parent.isPartyHpBar && modelData.showBubble !== false
                border.width: 1
                border.color: modelData.borderColor || "#80ffffff"
            }

            // A two-pixel outline gives actor names a stronger edge on bright terrain.
            Repeater {
                model: worldAnchor.isActorName ? [
                    Qt.point(0, -2),
                    Qt.point(-1, -1), Qt.point(0, -1), Qt.point(1, -1),
                    Qt.point(-2, 0), Qt.point(-1, 0), Qt.point(1, 0), Qt.point(2, 0),
                    Qt.point(-1, 1), Qt.point(0, 1), Qt.point(1, 1),
                    Qt.point(0, 2)
                ] : []

                delegate: UiText {
                    required property var modelData
                    x: label.x + modelData.x
                    y: label.y + modelData.y
                    width: label.width
                    text: label.text
                    textFormat: Text.PlainText
                    font: label.font
                    color: "#000000"
                    renderType: Text.NativeRendering
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            Text {
                id: label
                x: Math.floor(parent.bubblePaddingX / 2)
                y: Math.floor(parent.bubblePaddingY / 2)
                width: parent.textWidth
                text: modelData.text
                visible: !parent.isPartyHpBar
                color: modelData.foreground
                textFormat: parent.isActorName || parent.isActorText ? Text.PlainText : Text.AutoText
                font.family: parent.isActorName || parent.isActorText ? "Arial" : Qt.application.font.family
                font.pixelSize: modelData.fontPixelSize || 14
                font.bold: modelData.bold !== false
                font.hintingPreference: parent.isActorName ? Font.PreferFullHinting : Font.PreferDefaultHinting
                renderType: parent.isActorName ? Text.NativeRendering : Text.QtRendering
                style: modelData.outline ? Text.Outline : Text.Normal
                styleColor: "#000000"
                wrapMode: parent.wrapText ? Text.Wrap : Text.NoWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    UiRectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 42
        width: Math.min(540, parent.width - 80)
        height: 92
        color: root.uiTheme.panelBackground
        border.width: 1
        border.color: root.uiTheme.frame
        visible: uiState.loadingVisible

        Column {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 8

            Row {
                width: parent.width

                UiText {
                    id: loadingTitle
                    text: uiState.loadingProgress > 0 ? "Entering World" : uiState.modeName
                    color: root.uiTheme.uiText
                    font.pixelSize: 22
                    font.bold: true
                }

                Item {
                    width: Math.max(0, parent.width - loadingTitle.implicitWidth - loadingPercent.implicitWidth)
                    height: 1
                }

                UiText {
                    id: loadingPercent
                    text: Math.round(uiState.loadingProgress * 100) + "%"
                    color: root.uiTheme.uiText
                    font.pixelSize: 22
                    font.bold: true
                    visible: uiState.loadingProgress > 0
                }
            }

            UiText {
                width: parent.width
                text: uiState.loadingMessage
                color: root.uiTheme.uiText
                font.pixelSize: 16
                elide: Text.ElideRight
            }

            UiRectangle {
                width: parent.width
                height: 20
                color: root.uiTheme.controlBackground
                border.width: 1
                border.color: root.uiTheme.frame
                visible: uiState.loadingProgress > 0

                UiRectangle {
                    anchors.left: parent.left
                    anchors.leftMargin: 2
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.max(0, (parent.width - 4) * uiState.loadingProgress)
                    height: parent.height - 4
                    color: root.uiTheme.accent
                }
            }
        }
    }

    Item {
        id: uiLayer
        visible: !uiState.loadingVisible || uiState.loadingProgress <= 0
        x: 0
        y: 0
        width: root.uiScale > 0 ? root.width / root.uiScale : root.width
        height: root.uiScale > 0 ? root.height / root.uiScale : root.height
        scale: root.uiScale
        transformOrigin: Item.TopLeft

    Repeater {
        model: uiState.notifications

        delegate: UiRectangle {
            required property var modelData
            x: modelData.x
            y: modelData.y
            width: modelData.width
            height: modelData.height
            color: "#de1f2328"
            border.width: 1
            border.color: modelData.accent

            UiRectangle {
                x: 2
                y: 2
                width: parent.width - 4
                height: parent.height - 4
                color: "#ee2b3138"
                border.width: 1
                border.color: "#70ffffff"
            }

            UiText {
                anchors.centerIn: parent
                text: modelData.title
                color: "#f7f2df"
                font.pixelSize: 18
                font.bold: true
            }
        }
    }

    UiRectangle {
        x: uiState.npcMenuX
        y: uiState.npcMenuY
        width: uiState.npcMenuWidth
        height: uiState.npcMenuHeight
        color: "#f8f8f8"
        border.width: 1
        border.color: "#828282"
        visible: uiState.npcMenuVisible

        Column {
            x: 10
            y: 10
            width: parent.width - 20
            spacing: 0

            Repeater {
                model: uiState.npcMenuOptions

                delegate: UiRectangle {
                    required property int index
                    required property var modelData
                    width: parent.width
                    height: 18
                    color: index === uiState.npcMenuSelectedIndex
                        ? "#d1e0f4"
                        : (index === uiState.npcMenuHoverIndex ? "#e8eff8" : "transparent")
                    border.width: (index === uiState.npcMenuSelectedIndex || index === uiState.npcMenuHoverIndex) ? 1 : 0
                    border.color: "#a0b4cd"

                    UiText {
                        anchors.left: parent.left
                        anchors.leftMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 8
                        text: modelData
                        textFormat: Text.RichText
                        font.pixelSize: 12
                        clip: true
                    }
                }
            }
        }

        Repeater {
            model: uiState.npcMenuButtons

            delegate: UiRectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.npcMenuX
                y: (modelData.y || 0) - uiState.npcMenuY
                width: modelData.width || 0
                height: modelData.height || 0
                color: modelData.pressed ? "#c4c4c4" : "#f0f0f0"
                border.width: 1
                border.color: "#6e6e6e"

                UiText {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#000000"
                    font.pixelSize: 12
                }
            }
        }
    }

    UiRectangle {
        x: uiState.sayDialogX
        y: uiState.sayDialogY
        width: uiState.sayDialogWidth
        height: uiState.sayDialogHeight
        color: "#f8f8f8"
        border.width: 1
        border.color: "#828282"
        visible: uiState.sayDialogVisible

        UiText {
            x: 10
            y: 10
            width: parent.width - 20
            height: uiState.sayDialogHasAction ? (parent.height - 10 - 10 - 22 - 8) : (parent.height - 20)
            text: uiState.sayDialogText
            textFormat: Text.RichText
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            verticalAlignment: Text.AlignTop
        }

        UiRectangle {
            x: (uiState.sayDialogActionButton.x || 0) - uiState.sayDialogX
            y: (uiState.sayDialogActionButton.y || 0) - uiState.sayDialogY
            width: uiState.sayDialogActionButton.width || 0
            height: uiState.sayDialogActionButton.height || 0
            visible: uiState.sayDialogActionButton.visible || false
            color: (uiState.sayDialogActionButton.pressed || false) ? "#c4c4c4" : ((uiState.sayDialogActionButton.hovered || false) ? "#e4e4e4" : "#f0f0f0")
            border.width: 1
            border.color: "#6e6e6e"

            UiText {
                anchors.centerIn: parent
                text: uiState.sayDialogActionButton.label || ""
                color: "#000000"
                font.pixelSize: 12
            }
        }
    }

    UiRectangle {
        x: uiState.npcInputX
        y: uiState.npcInputY
        width: uiState.npcInputWidth
        height: uiState.npcInputHeight
        color: "#f8f8f8"
        border.width: 1
        border.color: "#828282"
        visible: uiState.npcInputVisible

        UiText {
            x: 10
            y: 9
            width: parent.width - 20
            text: uiState.npcInputLabel
            color: "#000000"
            font.pixelSize: 12
        }

        UiRectangle {
            x: 10
            y: 26
            width: parent.width - 20
            height: 22
            color: "#fff7c8"
            border.width: 1
            border.color: "#000000"

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 12
                text: uiState.npcInputText
                color: "#000000"
                font.pixelSize: 12
                elide: Text.ElideRight
            }
        }

        Repeater {
            model: uiState.npcInputButtons

            delegate: UiRectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.npcInputX
                y: (modelData.y || 0) - uiState.npcInputY
                width: modelData.width || 0
                height: modelData.height || 0
                color: modelData.pressed ? "#c4c4c4" : "#f0f0f0"
                border.width: 1
                border.color: "#6e6e6e"

                UiText {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#000000"
                    font.pixelSize: 12
                }
            }
        }
    }

    UiRectangle {
        x: uiState.chooseMenuX
        y: uiState.chooseMenuY
        width: uiState.chooseMenuWidth
        height: uiState.chooseMenuHeight
        color: "#ffffff"
        border.width: 1
        border.color: "#b8b8b8"
        visible: uiState.chooseMenuVisible

        Repeater {
            model: uiState.chooseMenuOptions

            delegate: UiRectangle {
                required property int index
                required property var modelData
                x: (parent.width - 221) / 2
                y: 12 + index * 23
                width: 221
                height: 20
                color: index === uiState.chooseMenuPressedIndex
                    ? "#b7c8de"
                    : (index === uiState.chooseMenuSelectedIndex ? "#cad8ea" : "#f1f1f1")
                border.width: 1
                border.color: "#8f8f8f"

                UiText {
                    anchors.centerIn: parent
                    text: modelData
                    color: "#1a1a1a"
                    font.pixelSize: 12
                }
            }
        }
    }

    UiRectangle {
        x: uiState.itemShopX
        y: uiState.itemShopY
        width: uiState.itemShopWidth
        height: uiState.itemShopHeight
        color: "#ececec"
        border.width: 1
        border.color: "#484848"
        visible: uiState.itemShopVisible

        UiRectangle {
            x: 0
            y: 0
            width: parent.width
            height: 17
            color: "#52657b"
        }

        UiText {
            x: 6
            y: 1
            text: uiState.itemShopData.title || uiState.itemShopTitle
            color: "#ffffff"
            font.pixelSize: 12
            font.bold: true
        }

        UiRectangle {
            x: 8
            y: 22
            width: parent.width - 16
            height: parent.height - 34
            color: "#f8f8f8"
            border.width: 1
            border.color: "#787878"

            Column {
                x: 1
                y: 1
                width: parent.width - 2
                spacing: 0

                UiRectangle {
                    width: parent.width
                    height: 17
                    color: "#dee5ed"

                    UiText {
                        x: 26
                        y: 2
                        width: (uiState.itemShopData.showQuantity || false) ? 160 : (parent.width - 110)
                        text: uiState.itemShopData.nameLabel || ""
                        color: "#1e1e1e"
                        font.pixelSize: 11
                    }

                    UiText {
                        x: 190
                        y: 2
                        width: 38
                        visible: uiState.itemShopData.showQuantity || false
                        text: uiState.itemShopData.quantityLabel || ""
                        color: "#1e1e1e"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                    }

                    UiText {
                        x: width - 60
                        y: 2
                        width: 54
                        text: uiState.itemShopData.priceLabel || ""
                        color: "#1e1e1e"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignRight
                    }
                }

                Repeater {
                    model: uiState.itemShopRows

                    delegate: UiRectangle {
                        required property var modelData
                        width: parent.width
                        height: 18
                        color: modelData.selected ? "#bccce2" : (modelData.hover ? "#e2eaf4" : "transparent")

                        UiText {
                            x: 24
                            y: 2
                            width: (uiState.itemShopData.showQuantity || false) ? 160 : (parent.width - 110)
                            text: modelData.name
                            color: "#1a1a1a"
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }

                        UiText {
                            x: 188
                            y: 2
                            width: 38
                            visible: uiState.itemShopData.showQuantity || false
                            text: modelData.quantity
                            color: "#303030"
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                        }

                        UiText {
                            x: parent.width - 76
                            y: 2
                            width: 70
                            text: modelData.price
                            color: "#1c3c62"
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }
            }
        }
    }

    UiRectangle {
        x: uiState.itemPurchaseX
        y: uiState.itemPurchaseY
        width: uiState.itemPurchaseWidth
        height: uiState.itemPurchaseHeight
        color: "#ececec"
        border.width: 1
        border.color: "#484848"
        visible: uiState.itemPurchaseVisible

        UiRectangle {
            x: 0
            y: 0
            width: parent.width
            height: 17
            color: "#52657b"
        }

        UiText {
            x: 6
            y: 1
            text: uiState.itemPurchaseData.title || ""
            color: "#ffffff"
            font.pixelSize: 12
            font.bold: true
        }

        UiRectangle {
            x: 8
            y: 22
            width: parent.width - 16
            height: parent.height - 80
            color: "#f8f8f8"
            border.width: 1
            border.color: "#787878"

            Column {
                x: 1
                y: 1
                width: parent.width - 2
                spacing: 0

                UiRectangle {
                    width: parent.width
                    height: 17
                    color: "#dee5ed"

                    UiText {
                        x: 4
                        y: 2
                        text: uiState.itemPurchaseData.nameLabel || ""
                        color: "#1e1e1e"
                        font.pixelSize: 11
                    }

                    UiText {
                        x: 126
                        y: 2
                        width: 34
                        text: uiState.itemPurchaseData.quantityLabel || ""
                        color: "#1e1e1e"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                    }

                    UiText {
                        x: width - 64
                        y: 2
                        width: 56
                        text: uiState.itemPurchaseData.amountLabel || ""
                        color: "#1e1e1e"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignRight
                    }
                }

                Repeater {
                    model: uiState.itemPurchaseRows

                    delegate: UiRectangle {
                        required property var modelData
                        width: parent.width
                        height: 18
                        color: modelData.selected ? "#bccce2" : (modelData.hover ? "#e2eaf4" : "transparent")

                        UiText {
                            x: 4
                            y: 2
                            width: 126
                            text: modelData.name
                            color: "#1a1a1a"
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }

                        UiText {
                            x: 126
                            y: 2
                            width: 34
                            text: modelData.quantity
                            color: "#303030"
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                        }

                        UiText {
                            x: parent.width - 64
                            y: 2
                            width: 56
                            text: modelData.cost
                            color: "#1c3c62"
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }
            }
        }

        UiText {
            x: 10
            y: height - 75
            text: uiState.itemPurchaseData.totalLabel || ""
            color: "#242424"
            font.pixelSize: 11
        }

        UiText {
            x: 90
            y: height - 75
            width: 138
            text: uiState.itemPurchaseTotal
            color: "#1c3c62"
            font.pixelSize: 11
            horizontalAlignment: Text.AlignRight
        }

        Repeater {
            model: uiState.itemPurchaseButtons

            delegate: UiRectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.itemPurchaseX
                y: (modelData.y || 0) - uiState.itemPurchaseY
                width: modelData.width || 0
                height: modelData.height || 0
                color: modelData.pressed ? "#aab9cd" : (modelData.hot ? "#c4d2e4" : "#dcdcdc")
                border.width: 1
                border.color: "#585858"

                UiText {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#181818"
                    font.pixelSize: 12
                }
            }
        }
    }

    UiRectangle {
        x: uiState.itemSellX
        y: uiState.itemSellY
        width: uiState.itemSellWidth
        height: uiState.itemSellHeight
        color: "#ececec"
        border.width: 1
        border.color: "#484848"
        visible: uiState.itemSellVisible

        UiRectangle {
            x: 0
            y: 0
            width: parent.width
            height: 17
            color: "#52657b"
        }

        UiText {
            x: 6
            y: 1
            text: uiState.itemSellData.title || ""
            color: "#ffffff"
            font.pixelSize: 12
            font.bold: true
        }

        UiRectangle {
            x: 8
            y: 22
            width: parent.width - 16
            height: parent.height - 80
            color: "#f8f8f8"
            border.width: 1
            border.color: "#787878"

            Column {
                x: 1
                y: 1
                width: parent.width - 2
                spacing: 0

                UiRectangle {
                    width: parent.width
                    height: 17
                    color: "#dee5ed"

                    UiText {
                        x: 4
                        y: 2
                        text: uiState.itemSellData.nameLabel || ""
                        color: "#1e1e1e"
                        font.pixelSize: 11
                    }

                    UiText {
                        x: 126
                        y: 2
                        width: 34
                        text: uiState.itemSellData.quantityLabel || ""
                        color: "#1e1e1e"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                    }

                    UiText {
                        x: width - 64
                        y: 2
                        width: 56
                        text: uiState.itemSellData.amountLabel || ""
                        color: "#1e1e1e"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignRight
                    }
                }

                Repeater {
                    model: uiState.itemSellRows

                    delegate: UiRectangle {
                        required property var modelData
                        width: parent.width
                        height: 18
                        color: modelData.selected ? "#bccce2" : (modelData.hover ? "#e2eaf4" : "transparent")

                        UiText {
                            x: 4
                            y: 2
                            width: 126
                            text: modelData.name
                            color: "#1a1a1a"
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }

                        UiText {
                            x: 126
                            y: 2
                            width: 34
                            text: modelData.quantity
                            color: "#303030"
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                        }

                        UiText {
                            x: parent.width - 64
                            y: 2
                            width: 56
                            text: modelData.gain
                            color: "#1c3c62"
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }
            }
        }

        UiText {
            x: 10
            y: height - 75
            text: uiState.itemSellData.totalLabel || ""
            color: "#242424"
            font.pixelSize: 11
        }

        UiText {
            x: 90
            y: height - 75
            width: 138
            text: uiState.itemSellTotal
            color: "#1c3c62"
            font.pixelSize: 11
            horizontalAlignment: Text.AlignRight
        }

        Repeater {
            model: uiState.itemSellButtons

            delegate: UiRectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.itemSellX
                y: (modelData.y || 0) - uiState.itemSellY
                width: modelData.width || 0
                height: modelData.height || 0
                color: modelData.pressed ? "#aab9cd" : (modelData.hot ? "#c4d2e4" : "#dcdcdc")
                border.width: 1
                border.color: "#585858"

                UiText {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#181818"
                    font.pixelSize: 12
                }
            }
        }
    }

    Item {
        id: experienceBars
        readonly property var barData: uiState.experienceBarData || ({})
        readonly property int frameWidth: barData.frameWidth || 2
        x: barData.x || 0
        y: barData.y || 0
        width: barData.width || 0
        height: barData.height || 0
        visible: barData.visible || false

        Repeater {
            model: [
                { progress: experienceBars.barData.baseProgress || 0, label: experienceBars.barData.baseProgressText || "0%", color: experienceBars.barData.baseColor },
                { progress: experienceBars.barData.jobProgress || 0, label: experienceBars.barData.jobProgressText || "0%", color: experienceBars.barData.jobColor }
            ]

            delegate: UiRectangle {
                required property int index
                required property var modelData
                readonly property int midpoint: Math.floor(experienceBars.width / 2)
                x: index === 0 ? 0 : midpoint
                width: index === 0 ? midpoint : experienceBars.width - midpoint
                height: experienceBars.height
                color: root.uiTheme.panelBackground

                UiRectangle {
                    x: experienceBars.frameWidth
                    y: experienceBars.frameWidth
                    width: Math.round(Math.max(0, parent.width - 2 * experienceBars.frameWidth) * modelData.progress)
                    height: Math.max(0, parent.height - 2 * experienceBars.frameWidth)
                    color: modelData.color || "transparent"
                }

                UiRectangle {
                    anchors.fill: parent
                    color: "transparent"
                    border.width: experienceBars.frameWidth
                    border.color: root.uiTheme.frame
                }

                UiText {
                    anchors.fill: parent
                    anchors.margins: experienceBars.frameWidth
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: modelData.label
                    color: "#ffffff"
                    font.pixelSize: experienceBars.barData.labelFontSize || 12
                    font.bold: true
                    style: Text.Outline
                    styleColor: "#000000"
                }
            }
        }
    }

    Item {
        x: uiState.shortCutX
        y: uiState.shortCutY
        width: uiState.shortCutWidth
        height: uiState.shortCutHeight
        visible: uiState.shortCutVisible

        Repeater {
            model: uiState.shortCutSlots

            delegate: UiRectangle {
                required property int index
                required property var modelData
                x: modelData.x
                y: modelData.y
                width: modelData.width
                height: modelData.height
                color: modelData.hover ? root.uiTheme.controlHover : root.uiTheme.panelBackground
                border.width: 1
                border.color: modelData.hover ? root.uiTheme.accent : root.uiTheme.frame

                Image {
                    id: shortCutIcon
                    anchors.fill: parent
                    anchors.margins: 1
                    fillMode: Image.PreserveAspectFit
                    smooth: false
                    source: modelData.isSkill
                        ? root.skillIconSource(modelData.skillId || 0)
                        : root.itemIconSource(modelData.itemId || 0)
                    visible: modelData.occupied && source !== ""
                }

                UiText {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 2
                    height: modelData.labelHeight
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: modelData.keyLabel
                    color: "#ffffff"
                    font.pixelSize: modelData.labelFontSize
                    font.bold: true
                    style: Text.Outline
                    styleColor: "#000000"
                }

                UiText {
                    anchors.right: shortCutIcon.right
                    anchors.top: shortCutIcon.top
                    anchors.rightMargin: 3
                    anchors.topMargin: 2
                    height: modelData.labelHeight
                    verticalAlignment: Text.AlignVCenter
                    text: modelData.quantityText
                    visible: text !== ""
                    color: "#ffffff"
                    font.pixelSize: modelData.labelFontSize
                    font.bold: true
                    style: Text.Outline
                    styleColor: "#000000"
                }
            }
        }
    }

    UiRectangle {
        x: uiState.basicInfoX
        y: uiState.basicInfoY
        width: uiState.basicInfoWidth
        height: uiState.basicInfoHeight
        color: uiState.basicInfoMini ? "#ece7d8" : "#ede9df"
        border.width: 1
        border.color: "#6b675f"
        visible: uiState.basicInfoVisible

        UiRectangle {
            x: 1
            y: 1
            width: parent.width - 2
            height: 16
            color: "#6e8194"
            border.width: 1
            border.color: "#4e5d6c"
        }

        UiText {
            x: uiState.basicInfoMini ? 9 : 17
            y: 3
            text: uiState.basicInfoMini ? (uiState.basicInfoData.name || "") : "Basic Info"
            color: "#ffffff"
            font.pixelSize: 12
            font.bold: true
        }

        Repeater {
            model: uiState.basicInfoData.systemButtons || []

            delegate: UiRectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.basicInfoX
                y: (modelData.y || 0) - uiState.basicInfoY
                width: modelData.width || 0
                height: modelData.height || 0
                color: "#d7d2c5"
                border.width: 1
                border.color: "#7f7a70"
                visible: modelData.visible || false

                UiText {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#000000"
                    font.pixelSize: 8
                    font.bold: true
                }
            }
        }

        UiText {
            x: 17
            y: 18
            visible: uiState.basicInfoMini
            text: uiState.basicInfoData.miniHeaderText || ""
            color: "#000000"
            font.pixelSize: 11
        }

        UiText {
            x: 17
            y: 33
            visible: uiState.basicInfoMini
            text: uiState.basicInfoData.miniStatusText || ""
            color: "#000000"
            font.pixelSize: 11
        }

        UiText {
            x: 9
            y: 24
            visible: !uiState.basicInfoMini
            text: uiState.basicInfoData.name || ""
            color: "#000000"
            font.pixelSize: 12
        }

        UiText {
            x: 9
            y: 38
            visible: !uiState.basicInfoMini
            text: uiState.basicInfoData.jobName || ""
            color: "#000000"
            font.pixelSize: 12
        }

        UiRectangle {
            x: 110
            y: 22
            width: 85
            height: 9
            visible: !uiState.basicInfoMini
            color: "#403c3c"
            border.width: 1
            border.color: "#202020"

            UiRectangle {
                x: 1
                y: 1
                width: Math.max(0, (parent.width - 2) * ((uiState.basicInfoData.maxHp || 0) > 0 ? (uiState.basicInfoData.hp || 0) / uiState.basicInfoData.maxHp : 0))
                height: parent.height - 2
                color: ((uiState.basicInfoData.maxHp || 0) > 0 && (uiState.basicInfoData.hp || 0) * 100 < uiState.basicInfoData.maxHp * 25) ? "#d04848" : "#d06060"
            }
        }

        UiRectangle {
            x: 110
            y: 43
            width: 85
            height: 9
            visible: !uiState.basicInfoMini
            color: "#403c3c"
            border.width: 1
            border.color: "#202020"

            UiRectangle {
                x: 1
                y: 1
                width: Math.max(0, (parent.width - 2) * ((uiState.basicInfoData.maxSp || 0) > 0 ? (uiState.basicInfoData.sp || 0) / uiState.basicInfoData.maxSp : 0))
                height: parent.height - 2
                color: "#4a78d8"
            }
        }

        UiText {
            x: 95
            y: 31
            visible: !uiState.basicInfoMini
            text: uiState.basicInfoData.hpText || ""
            color: "#000000"
            font.pixelSize: 10
        }

        UiText {
            x: 95
            y: 52
            visible: !uiState.basicInfoMini
            text: uiState.basicInfoData.spText || ""
            color: "#000000"
            font.pixelSize: 10
        }

        UiText {
            x: 17
            y: 72
            visible: !uiState.basicInfoMini
            text: uiState.basicInfoData.baseLevelText || ""
            color: "#000000"
            font.pixelSize: 11
        }

        UiRectangle {
            x: 84
            y: 77
            width: 102
            height: 6
            visible: !uiState.basicInfoMini
            color: "#ffffff"
            border.width: 1
            border.color: "#808080"

            UiRectangle {
                x: 1
                y: 1
                width: Math.max(0, (parent.width - 2) * ((uiState.basicInfoData.expPercent || 0) / 100.0))
                height: parent.height - 2
                color: "#4fd17f"
            }
        }

        UiText {
            x: 17
            y: 84
            visible: !uiState.basicInfoMini
            text: uiState.basicInfoData.jobLevelText || ""
            color: "#000000"
            font.pixelSize: 11
        }

        UiRectangle {
            x: 84
            y: 87
            width: 102
            height: 6
            visible: !uiState.basicInfoMini
            color: "#ffffff"
            border.width: 1
            border.color: "#808080"

            UiRectangle {
                x: 1
                y: 1
                width: Math.max(0, (parent.width - 2) * ((uiState.basicInfoData.jobExpPercent || 0) / 100.0))
                height: parent.height - 2
                color: "#6aa8ff"
            }
        }

        UiText {
            x: 5
            y: 103
            visible: !uiState.basicInfoMini
            text: uiState.basicInfoData.weightText || ""
            color: ((uiState.basicInfoData.maxWeight || 0) > 0 && (uiState.basicInfoData.weight || 0) * 100 >= uiState.basicInfoData.maxWeight * 50) ? "#ff0000" : "#000000"
            font.pixelSize: 11
        }

        UiText {
            x: 107
            y: 103
            visible: !uiState.basicInfoMini
            text: uiState.basicInfoData.moneyText || ""
            color: "#000000"
            font.pixelSize: 11
        }

        Repeater {
            model: uiState.basicInfoData.menuButtons || []

            delegate: UiRectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.basicInfoX
                y: (modelData.y || 0) - uiState.basicInfoY
                width: modelData.width || 0
                height: modelData.height || 0
                color: "#d7d2c5"
                border.width: 1
                border.color: "#7f7a70"
                visible: modelData.visible || false

                UiText {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#000000"
                    font.pixelSize: 8
                    font.bold: true
                }
            }
        }
    }

    UiRectangle {
        x: uiState.statusX
        y: uiState.statusY
        width: uiState.statusWidth
        height: uiState.statusHeight
        color: uiState.statusMini ? "#ece7d8" : "#ede9df"
        border.width: 1
        border.color: "#6b675f"
        visible: uiState.statusVisible

        UiRectangle {
            x: 1
            y: 1
            width: parent.width - 2
            height: 16
            color: uiState.statusMini ? "#d5d0c2" : "#6e8194"
            border.width: 1
            border.color: uiState.statusMini ? "#8b877b" : "#4e5d6c"
        }

        UiText {
            x: 17
            y: 3
            text: uiState.statusData.title || ""
            color: uiState.statusMini ? "#000000" : "#ffffff"
            font.pixelSize: 12
            font.bold: true
        }

        Repeater {
            model: uiState.statusData.systemButtons || []

            delegate: UiRectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.statusX
                y: (modelData.y || 0) - uiState.statusY
                width: modelData.width || 0
                height: modelData.height || 0
                color: "#d7d2c5"
                border.width: 1
                border.color: "#7f7a70"
                visible: modelData.visible || false

                UiText {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#000000"
                    font.pixelSize: 8
                    font.bold: true
                }
            }
        }

        UiText {
            x: 96
            y: 3
            visible: uiState.statusMini
            text: uiState.statusData.miniPointsText || ""
            color: "#000000"
            font.pixelSize: 11
        }

        UiRectangle {
            x: 0
            y: 17
            width: parent.width
            height: parent.height - 17
            color: "#e7e2d6"
            border.width: 0
            visible: !uiState.statusMini

            UiRectangle {
                x: 0
                y: 0
                width: 20
                height: parent.height
                color: "#d3cdbf"
            }

            Repeater {
                model: uiState.statusData.pageTabs || []

                delegate: UiRectangle {
                    required property var modelData
                    x: (modelData.x || 0) - uiState.statusX
                    y: (modelData.y || 0) - uiState.statusY - 17
                    width: modelData.width || 0
                    height: modelData.height || 0
                    color: modelData.active ? "#ebe7db" : "#c9c2b2"
                    border.width: 1
                    border.color: "#8c8578"
                    visible: modelData.visible || false

                    UiText {
                        anchors.centerIn: parent
                        text: modelData.label || ""
                        color: "#000000"
                        font.pixelSize: 10
                        font.bold: modelData.active || false
                    }
                }
            }

            Column {
                x: 28
                y: 5
                spacing: 2
                visible: uiState.statusPage === 0

                Repeater {
                    model: uiState.statusData.stats || []

                    delegate: Item {
                        required property var modelData
                        width: 90
                        height: 14

                        UiText {
                            x: 0
                            y: 0
                            text: modelData.label
                            color: "#000000"
                            font.pixelSize: 11
                            font.bold: true
                        }

                        UiText {
                            x: 26
                            y: 0
                            width: 34
                            text: modelData.value
                            color: "#000000"
                            font.pixelSize: 11
                        }

                        UiRectangle {
                            x: 64
                            y: 1
                            width: modelData.increaseWidth || 0
                            height: modelData.increaseHeight || 0
                            visible: modelData.canIncrease
                            color: "#d8d8d8"
                            border.width: 1
                            border.color: "#7f7a70"

                            UiText {
                                anchors.centerIn: parent
                                text: modelData.increaseLabel || ""
                                color: "#000000"
                                font.pixelSize: 8
                                font.bold: true
                            }
                        }

                        UiText {
                            x: 78
                            y: 0
                            width: 10
                            text: modelData.cost > 0 ? modelData.cost : ""
                            color: "#4a4a4a"
                            font.pixelSize: 10
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }
            }

            UiText {
                x: 132
                y: 10
                visible: uiState.statusPage === 0
                text: "Atk"
                color: "#000000"
                font.pixelSize: 11
            }

            UiText {
                x: 152
                y: 10
                width: 40
                visible: uiState.statusPage === 0
                text: uiState.statusData.attackText || ""
                color: "#000000"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
            }

            UiText {
                x: 132
                y: 26
                visible: uiState.statusPage === 0
                text: "Matk"
                color: "#000000"
                font.pixelSize: 11
            }

            UiText {
                x: 152
                y: 26
                width: 40
                visible: uiState.statusPage === 0
                text: uiState.statusData.matkText || ""
                color: "#000000"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
            }

            UiText {
                x: 132
                y: 42
                visible: uiState.statusPage === 0
                text: "Hit"
                color: "#000000"
                font.pixelSize: 11
            }

            UiText {
                x: 152
                y: 42
                width: 40
                visible: uiState.statusPage === 0
                text: (uiState.statusData.hit || 0).toString()
                color: "#000000"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
            }

            UiText {
                x: 132
                y: 58
                visible: uiState.statusPage === 0
                text: "Crit"
                color: "#000000"
                font.pixelSize: 11
            }

            UiText {
                x: 152
                y: 58
                width: 40
                visible: uiState.statusPage === 0
                text: (uiState.statusData.critical || 0).toString()
                color: "#000000"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
            }

            UiText {
                x: 204
                y: 74
                visible: uiState.statusPage === 0
                text: "Pts"
                color: "#000000"
                font.pixelSize: 11
                font.bold: true
            }

            UiText {
                x: 243
                y: 74
                width: 30
                visible: uiState.statusPage === 0
                text: (uiState.statusData.statusPoint || 0).toString()
                color: "#000000"
                font.pixelSize: 11
                font.bold: true
                horizontalAlignment: Text.AlignRight
            }

            UiText {
                x: 204
                y: 10
                visible: uiState.statusPage === 0
                text: "Def"
                color: "#000000"
                font.pixelSize: 11
            }

            UiText {
                x: 233
                y: 10
                width: 40
                visible: uiState.statusPage === 0
                text: uiState.statusData.itemDefText || ""
                color: "#000000"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
            }

            UiText {
                x: 204
                y: 26
                visible: uiState.statusPage === 0
                text: "Mdef"
                color: "#000000"
                font.pixelSize: 11
            }

            UiText {
                x: 233
                y: 26
                width: 40
                visible: uiState.statusPage === 0
                text: uiState.statusData.itemMdefText || ""
                color: "#000000"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
            }

            UiText {
                x: 204
                y: 42
                visible: uiState.statusPage === 0
                text: "Flee"
                color: "#000000"
                font.pixelSize: 11
            }

            UiText {
                x: 233
                y: 42
                width: 40
                visible: uiState.statusPage === 0
                text: uiState.statusData.fleeText || ""
                color: "#000000"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
            }

            UiText {
                x: 204
                y: 58
                visible: uiState.statusPage === 0
                text: "Aspd"
                color: "#000000"
                font.pixelSize: 11
            }

            UiText {
                x: 233
                y: 58
                width: 40
                visible: uiState.statusPage === 0
                text: uiState.statusData.aspdText || ""
                color: "#000000"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
            }
        }
    }

    Item {
        id: chatWindowChrome
        x: uiState.chatWindowX
        y: uiState.chatWindowY
        width: uiState.chatWindowWidth
        height: uiState.chatWindowHeight
        visible: uiState.chatWindowVisible

        readonly property var chatUi: uiState.chatWindowUi || ({})
        readonly property var scrollBar: uiState.chatWindowScrollBar || ({})
        readonly property string chatFontFamily: "Segoe UI"
        readonly property int chatFontSize: chatUi.fontPixelSize || 14
        readonly property real chromeOpacity: root.chatChromeOpacity(chatUi)
        readonly property color navy: root.uiTheme.panelBackground
        readonly property color background: Qt.rgba(navy.r, navy.g, navy.b, chromeOpacity)
        readonly property bool scrollBarVisible: scrollBar.visible || false
        readonly property int scrollBarWidth: scrollBarVisible ? 8 : 0
        readonly property int scrollBarGap: scrollBarVisible ? 4 : 0

        UiRectangle {
            readonly property var bounds: chatWindowChrome.chatUi.body || ({})
            x: bounds.x || 0
            y: bounds.y || 0
            width: bounds.width || 0
            height: bounds.height || 0
            color: chatWindowChrome.background
            border.width: 1
            border.color: root.uiTheme.frame
        }

        Repeater {
            model: chatWindowChrome.chatUi.tabs || []

            delegate: UiRectangle {
                required property var modelData
                x: modelData.bounds.x
                y: modelData.bounds.y
                width: modelData.bounds.width
                height: modelData.bounds.height
                color: chatWindowChrome.background
                border.width: 1
                border.color: modelData.active ? root.uiTheme.accent : root.uiTheme.frame

                Text {
                    anchors.centerIn: parent
                    text: modelData.label
                    color: "#ffffff"
                    font.family: root.uiTheme.uiFontFamily
                    font.pixelSize: 14
                    font.bold: true
                }
            }
        }

        Item {
            id: chatHistory
            readonly property var bounds: chatWindowChrome.chatUi.history || ({})
            x: bounds.x || 0
            y: bounds.y || 0
            width: bounds.width || 0
            height: bounds.height || 0
            clip: true

            Column {
                id: chatLinesColumn
                x: 4
                y: 4
                width: Math.max(0, chatHistory.width - 8 - chatWindowChrome.scrollBarWidth - chatWindowChrome.scrollBarGap)
                spacing: 2

                Repeater {
                    model: uiState.chatWindowLines

                    delegate: Text {
                        required property var modelData
                        width: chatLinesColumn.width
                        height: Math.max(19, implicitHeight)
                        text: modelData.styledText
                        textFormat: Text.StyledText
                        color: modelData.color
                        font.family: chatWindowChrome.chatFontFamily
                        font.pixelSize: chatWindowChrome.chatFontSize
                        wrapMode: Text.WordWrap
                    }
                }
            }

            UiRectangle {
                id: chatScrollTrack
                x: parent.width - 4 - width
                y: 4
                width: 8
                height: Math.max(0, parent.height - 8)
                visible: chatWindowChrome.scrollBarVisible
                color: "transparent"
                border.width: 1
                border.color: root.uiTheme.frame

                UiRectangle {
                    readonly property int totalLines: Math.max(1, chatWindowChrome.scrollBar.totalLines || 0)
                    readonly property int visibleLines: Math.max(1, chatWindowChrome.scrollBar.visibleLineCount || 0)
                    readonly property int firstLine: Math.max(0, chatWindowChrome.scrollBar.firstVisibleLine || 0)
                    readonly property int thumbHeight: Math.min(chatScrollTrack.height, Math.max(18, Math.round(chatScrollTrack.height * visibleLines / totalLines)))
                    readonly property int maxTravel: Math.max(0, chatScrollTrack.height - thumbHeight)
                    x: 1
                    y: Math.round(maxTravel * firstLine / Math.max(1, totalLines - visibleLines))
                    width: parent.width - 2
                    height: thumbHeight
                    color: root.uiTheme.frame
                }
            }
        }

        UiRectangle {
            readonly property var bounds: chatWindowChrome.chatUi.whisperInput || ({})
            x: bounds.x || 0
            y: bounds.y || 0
            width: bounds.width || 0
            height: bounds.height || 0
            color: "transparent"
            border.width: 1
            border.color: uiState.chatWindowWhisperInputActive ? root.uiTheme.accent : root.uiTheme.frame

            Text {
                x: 10
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(0, parent.width - 20)
                text: {
                    const active = uiState.chatWindowWhisperInputActive
                    const value = uiState.chatWindowWhisperTargetText
                    return value.length > 0 || active ? value + (active ? "_" : "") : "Whisper..."
                }
                color: "#ffffff"
                font.family: chatWindowChrome.chatFontFamily
                font.pixelSize: chatWindowChrome.chatFontSize
                font.bold: false
                elide: Text.ElideLeft
            }
        }

        UiRectangle {
            readonly property var bounds: chatWindowChrome.chatUi.messageInput || ({})
            x: bounds.x || 0
            y: bounds.y || 0
            width: bounds.width || 0
            height: bounds.height || 0
            color: "transparent"
            border.width: 1
            border.color: uiState.chatWindowMessageInputActive ? root.uiTheme.accent : root.uiTheme.frame

            Text {
                x: 10
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(0, parent.width - 20)
                text: {
                    const active = uiState.chatWindowMessageInputActive
                    const value = uiState.chatWindowInputText
                    return value.length > 0 || active ? value + (active ? "_" : "") : "Mssage..."
                }
                color: "#ffffff"
                font.family: chatWindowChrome.chatFontFamily
                font.pixelSize: chatWindowChrome.chatFontSize
                font.bold: false
                elide: Text.ElideLeft
            }
        }
    }

    UiRectangle {
        id: chatSettings
        readonly property var chatUi: uiState.chatWindowUi || ({})
        visible: uiState.chatWindowVisible && (chatUi.configVisible || false)
        x: chatUi.configX || 0
        y: chatUi.configY || 0
        width: chatUi.configWidth || 0
        height: chatUi.configHeight || 0
        color: chatWindowChrome.background
        border.width: 1
        border.color: root.uiTheme.frame

        UiText {
            x: 12
            y: 14
            text: "Chat settings"
            color: root.uiTheme.frame
            font.pixelSize: 15
        }

        Text {
            x: 12
            y: 43
            text: "Font size and background opacity"
            color: "#cbd4e3"
            font.family: "Segoe UI"
            font.pixelSize: 12
        }

        Text {
            readonly property var bounds: chatSettings.chatUi.fontValue || ({})
            x: bounds.x || 0
            y: bounds.y || 0
            height: bounds.height || 0
            text: "Font size: " + (chatSettings.chatUi.fontPixelSize || 14)
            verticalAlignment: Text.AlignVCenter
            color: "#ffffff"
            font.family: "Segoe UI"
            font.pixelSize: 13
        }

        Repeater {
            model: [
                { label: "-", bounds: chatSettings.chatUi.fontMinusButton || ({}) },
                { label: "+", bounds: chatSettings.chatUi.fontPlusButton || ({}) }
            ]

            delegate: UiRectangle {
                required property var modelData
                x: modelData.bounds.x || 0
                y: modelData.bounds.y || 0
                width: modelData.bounds.width || 0
                height: modelData.bounds.height || 0
                color: "transparent"
                border.width: 1
                border.color: root.uiTheme.frame

                Text {
                    anchors.centerIn: parent
                    text: modelData.label
                    color: "#ffffff"
                    font.family: "Segoe UI"
                    font.pixelSize: 16
                }
            }
        }

        Text {
            readonly property var bounds: chatSettings.chatUi.opacityValue || ({})
            x: bounds.x || 0
            y: bounds.y || 0
            height: bounds.height || 0
            text: "Background opacity: " + (chatSettings.chatUi.windowOpacityPercent || 50) + "%"
            verticalAlignment: Text.AlignVCenter
            color: "#ffffff"
            font.family: "Segoe UI"
            font.pixelSize: 13
        }

        UiRectangle {
            readonly property var bounds: chatSettings.chatUi.opacityTrack || ({})
            x: bounds.x || 0
            y: bounds.y || 0
            width: bounds.width || 0
            height: bounds.height || 0
            color: "transparent"
            border.width: 1
            border.color: root.uiTheme.frame

            UiRectangle {
                x: Math.max(0, Math.min(parent.width - 14, parent.width * (((chatSettings.chatUi.windowOpacityPercent || 50) - 20) / 80) - 7))
                y: -5
                width: 14
                height: parent.height + 10
                color: root.uiTheme.frame
            }
        }
    }

    UiRectangle {
        x: uiState.rechargeGaugeX
        y: uiState.rechargeGaugeY
        width: uiState.rechargeGaugeWidth
        height: uiState.rechargeGaugeHeight
        color: "#523f2d"
        border.width: 1
        border.color: "#523f2d"
        visible: uiState.rechargeGaugeVisible

        UiRectangle {
            x: 1
            y: 1
            width: parent.width - 2
            height: parent.height - 2
            color: "#f4efe4"
        }

        UiRectangle {
            x: 1
            y: 1
            width: Math.max(0, (parent.width - 2) * ((uiState.rechargeGaugeTotal || 0) > 0
                ? (uiState.rechargeGaugeAmount || 0) / uiState.rechargeGaugeTotal : 0))
            height: parent.height - 2
            color: "#307830"
        }
    }

    UiRectangle {
        x: uiState.minimapX
        y: uiState.minimapY
        width: uiState.minimapWidth
        height: uiState.minimapHeight
        color: "#d1d8e4"
        border.width: 1
        border.color: "#394256"
        visible: uiState.minimapVisible

        UiRectangle {
            x: 1
            y: 1
            width: parent.width - 2
            height: 16
            color: "#62729e"
        }

        UiRectangle {
            id: minimapMapFrame
            x: (uiState.minimapData.mapX || 0) - parent.x
            y: (uiState.minimapData.mapY || 0) - parent.y
            width: uiState.minimapData.mapWidth || 0
            height: uiState.minimapData.mapHeight || 0
            color: "#12161d"
            border.width: 1
            border.color: "#485060"
            clip: true

            Image {
                anchors.fill: parent
                fillMode: Image.Stretch
                smooth: false
                cache: false
                source: parent.visible ? ("image://openmidgard/minimap?rev=" + (uiState.minimapData.imageRevision || 0)) : ""
            }

            Repeater {
                model: uiState.minimapData.markers || []

                delegate: UiRectangle {
                    required property var modelData
                    x: modelData.x - (uiState.minimapData.mapX || 0) - modelData.radius
                    y: modelData.y - (uiState.minimapData.mapY || 0) - modelData.radius
                    width: modelData.radius * 2 + 1
                    height: modelData.radius * 2 + 1
                    radius: width / 2
                    color: modelData.color
                    border.width: 1
                    border.color: "#181818"
                }
            }

            Item {
                    property real headingDegrees: ((180 - (((uiState.minimapData.playerDirection || 0) % 8) * 45)) + 360) % 360

                    width: 19
                    height: 19
                visible: uiState.minimapData.playerVisible || false
                x: (uiState.minimapData.playerX || 0) - (uiState.minimapData.mapX || 0) - width / 2
                y: (uiState.minimapData.playerY || 0) - (uiState.minimapData.mapY || 0) - height / 2
                transformOrigin: Item.Center
                rotation: headingDegrees

                Canvas {
                    anchors.fill: parent
                    antialiasing: false

                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.reset()
                        ctx.fillStyle = "#ffffff"
                        ctx.strokeStyle = "#000000"
                        ctx.lineWidth = 1
                        ctx.beginPath()
                        ctx.moveTo(width / 2, 0.5)
                        ctx.lineTo(width - 1.5, height - 3.5)
                        ctx.lineTo(width / 2, height - 6.5)
                        ctx.lineTo(1.5, height - 3.5)
                        ctx.closePath()
                        ctx.fill()
                        ctx.stroke()
                    }
                }
            }
        }

        UiText {
            x: 18
            y: 2
            width: Math.max(0, parent.width - 40)
            text: uiState.minimapData.title || ""
            color: "#ffffff"
            font.pixelSize: 12
            font.bold: true
            elide: Text.ElideRight
        }

        UiText {
            x: (uiState.minimapData.coordsX || 0) - parent.x
            y: (uiState.minimapData.coordsY || 0) - parent.y
            width: Math.max(0, uiState.minimapData.coordsWidth || 0)
            horizontalAlignment: Text.AlignRight
            text: uiState.minimapData.coordsText || ""
            color: "#101010"
            font.pixelSize: 11
            elide: Text.ElideRight
        }

        UiRectangle {
            x: (uiState.minimapData.closeX || 0) - parent.x
            y: (uiState.minimapData.closeY || 0) - parent.y
            width: Math.max(10, uiState.minimapData.closeWidth || 0)
            height: Math.max(10, uiState.minimapData.closeHeight || 0)
            color: uiState.minimapData.closePressed ? "#b8c7da" : "#dde4ef"
            border.width: 1
            border.color: "#4d5662"

            UiText {
                anchors.centerIn: parent
                text: uiState.minimapData.closeLabel || ""
                color: "#18202a"
                font.pixelSize: 10
                font.bold: true
            }
        }
    }

    Item {
        x: Math.max(0, Math.floor(parent.width) - width - 6)
        y: uiState.minimapVisible && uiState.minimapX + uiState.minimapWidth > x
            ? Math.max(6, uiState.minimapY + uiState.minimapHeight + 6)
            : 6
        width: 56
        height: {
            const iconCount = (uiState.statusIcons || []).length
            return iconCount > 0 ? (iconCount * 56) + ((iconCount - 1) * 6) : 0
        }
        visible: (uiState.statusIcons || []).length > 0
        z: 72

        Column {
            anchors.fill: parent
            spacing: 6

            Repeater {
                model: uiState.statusIcons || []

                delegate: Item {
                    required property var modelData
                    property real tintFraction: statusExpiryTintFraction(modelData)

                    width: 56
                    height: 56

                    Item {
                        anchors.fill: parent
                        clip: true

                        Image {
                            id: statusIconImage
                            anchors.fill: parent
                            source: statusIconSource(modelData.statusType || 0)
                            visible: source !== "" && status === Image.Ready
                            smooth: false
                            cache: false
                        }

                        UiRectangle {
                            anchors.fill: parent
                            visible: !statusIconImage.visible
                            color: "#556574"
                        }

                        UiText {
                            anchors.centerIn: parent
                            visible: !statusIconImage.visible
                            text: modelData.shortName || "?"
                            color: "#f6f1e0"
                            font.pixelSize: 18
                            font.bold: true
                        }

                        UiRectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: Math.round(parent.height * tintFraction)
                            visible: tintFraction > 0
                            color: "#c92f2f"
                            opacity: 0.2 + (0.45 * tintFraction)
                        }
                    }
                }
            }
        }
    }

    UiRectangle {
        x: uiState.inventoryX
        y: uiState.inventoryY
        width: uiState.inventoryWidth
        height: uiState.inventoryHeight
        color: "#ede9df"
        border.width: 1
        border.color: "#6b675f"
        visible: uiState.inventoryVisible

        UiRectangle {
            x: 1
            y: 1
            width: parent.width - 2
            height: 16
            color: "#6e8194"
            border.width: 1
            border.color: "#4e5d6c"
        }

        UiText {
            x: 17
            y: 3
            text: uiState.inventoryData.title || "Inventory"
            color: "#ffffff"
            font.pixelSize: 12
            font.bold: true
        }

        Repeater {
            model: uiState.inventoryData.systemButtons || []

            delegate: UiRectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.inventoryX
                y: (modelData.y || 0) - uiState.inventoryY
                width: modelData.width || 0
                height: modelData.height || 0
                color: "#d7d2c5"
                border.width: 1
                border.color: "#7f7a70"
                visible: modelData.visible || false

                UiText {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#000000"
                    font.pixelSize: 8
                    font.bold: true
                }
            }
        }

        UiRectangle {
            x: 0
            y: 17
            width: parent.width
            height: parent.height - 17
            color: "#e7e2d6"
            visible: !uiState.inventoryMini

            Repeater {
                model: uiState.inventoryData.tabs || []

                delegate: UiRectangle {
                    required property var modelData
                    x: (modelData.x || 0) - uiState.inventoryX
                    y: (modelData.y || 0) - uiState.inventoryY - 17
                    width: modelData.width || 0
                    height: modelData.height || 0
                    color: modelData.active ? "#ebe7db" : "#c9c2b2"
                    border.width: 1
                    border.color: "#8c8578"
                    visible: modelData.visible || false

                    UiText {
                        anchors.centerIn: parent
                        text: modelData.label || ""
                        color: "#000000"
                        font.pixelSize: 9
                        font.bold: modelData.active || false
                    }
                }
            }

            Repeater {
                model: uiState.inventoryData.slots || []

                delegate: UiRectangle {
                    required property var modelData
                    x: modelData.x - uiState.inventoryX
                    y: modelData.y - uiState.inventoryY - 17
                    width: modelData.width
                    height: modelData.height
                    color: modelData.hovered ? "#d7dff0" : "#f5f2ea"
                    border.width: 1
                    border.color: modelData.hovered ? "#7e95bf" : "#a69f91"

                    Image {
                        id: inventoryIcon
                        anchors.centerIn: parent
                        width: Math.max(1, parent.width - 4)
                        height: Math.max(1, parent.height - 4)
                        fillMode: Image.PreserveAspectFit
                        smooth: false
                        cache: false
                        source: modelData.occupied && (modelData.itemId || 0) > 0 ? root.itemIconSource(modelData.itemId || 0) : ""
                        visible: source !== "" && status === Image.Ready
                    }

                    UiText {
                        anchors.centerIn: parent
                        width: parent.width - 4
                        text: modelData.occupied ? modelData.label : ""
                        color: "#000000"
                        font.pixelSize: 9
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        visible: modelData.occupied && !inventoryIcon.visible
                    }

                    UiText {
                        anchors.right: parent.right
                        anchors.rightMargin: 3
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 2
                        text: (modelData.count || 0) > 1 ? modelData.count : ""
                        color: "#222e50"
                        font.pixelSize: 9
                        font.bold: true
                    }
                }
            }

            UiRectangle {
                x: (uiState.inventoryData.scrollTrackX || 0) - uiState.inventoryX
                y: (uiState.inventoryData.scrollTrackY || 0) - uiState.inventoryY - 17
                width: uiState.inventoryData.scrollTrackWidth || 0
                height: uiState.inventoryData.scrollTrackHeight || 0
                visible: uiState.inventoryData.scrollBarVisible || false
                color: "#e3e7ee"
                border.width: 1
                border.color: "#a4adbd"

                UiRectangle {
                    x: (uiState.inventoryData.scrollThumbX || 0) - (uiState.inventoryData.scrollTrackX || 0)
                    y: (uiState.inventoryData.scrollThumbY || 0) - (uiState.inventoryData.scrollTrackY || 0)
                    width: uiState.inventoryData.scrollThumbWidth || 0
                    height: uiState.inventoryData.scrollThumbHeight || 0
                    color: "#8192c7"
                    border.width: 1
                    border.color: "#3f5684"
                }
            }

            UiRectangle {
                x: 0
                y: parent.height - height
                width: parent.width
                height: 21
                color: "#ddd7ca"
                border.width: 1
                border.color: "#bcb4a7"

                UiText {
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: (uiState.inventoryData.currentItemCount || 0) + " / " + (uiState.inventoryData.maxItemCount || 0)
                    color: "#4a4a4a"
                    font.pixelSize: 10
                }
            }
        }
    }

    UiRectangle {
        x: uiState.storageX
        y: uiState.storageY
        width: uiState.storageWidth
        height: uiState.storageHeight
        color: "#ede7dc"
        border.width: 1
        border.color: "#7a6d57"
        visible: uiState.storageVisible

        UiRectangle {
            x: 1
            y: 1
            width: parent.width - 2
            height: 16
            color: "#8c7551"
            border.width: 1
            border.color: "#655238"
        }

        UiText {
            x: 17
            y: 3
            text: uiState.storageData.title || "Kafra Storage"
            color: "#fff8ee"
            font.pixelSize: 12
            font.bold: true
        }

        Repeater {
            model: uiState.storageData.systemButtons || []

            delegate: UiRectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.storageX
                y: (modelData.y || 0) - uiState.storageY
                width: modelData.width || 0
                height: modelData.height || 0
                color: "#ddd0b8"
                border.width: 1
                border.color: "#8a775a"
                visible: modelData.visible || false

                UiText {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#000000"
                    font.pixelSize: 8
                    font.bold: true
                }
            }
        }

        UiRectangle {
            x: 0
            y: 17
            width: parent.width
            height: parent.height - 17
            color: "#e8decc"
            visible: !uiState.storageMini

            Repeater {
                model: uiState.storageData.tabs || []

                delegate: UiRectangle {
                    required property var modelData
                    x: (modelData.x || 0) - uiState.storageX
                    y: (modelData.y || 0) - uiState.storageY - 17
                    width: modelData.width || 0
                    height: modelData.height || 0
                    color: modelData.active ? "#efe6d5" : "#cfbea1"
                    border.width: 1
                    border.color: "#8d7b62"
                    visible: modelData.visible || false

                    UiText {
                        anchors.centerIn: parent
                        text: modelData.label || ""
                        color: "#000000"
                        font.pixelSize: 9
                        font.bold: modelData.active || false
                    }
                }
            }

            UiRectangle {
                x: 36
                y: 0
                width: parent.width - 56
                height: 18
                color: "#dfd6c6"
                border.width: 1
                border.color: "#b6a891"

                UiText {
                    x: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Item"
                    color: "#4a3a24"
                    font.pixelSize: 10
                    font.bold: true
                }

                UiText {
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Qty"
                    color: "#4a3a24"
                    font.pixelSize: 10
                    font.bold: true
                }
            }

            Repeater {
                model: uiState.storageData.slots || []

                delegate: UiRectangle {
                    required property var modelData
                    x: modelData.x - uiState.storageX
                    y: modelData.y - uiState.storageY - 17
                    width: modelData.width
                    height: modelData.height
                    color: modelData.hovered ? "#e5ddce" : "#f5eee3"
                    border.width: 1
                    border.color: modelData.hovered ? "#9b7f56" : "#ab9c86"

                    Image {
                        id: storageIcon
                        x: 5
                        y: Math.floor((parent.height - height) / 2)
                        width: 20
                        height: 20
                        fillMode: Image.PreserveAspectFit
                        smooth: false
                        cache: false
                        source: modelData.occupied && (modelData.itemId || 0) > 0 ? root.itemIconSource(modelData.itemId || 0) : ""
                        visible: source !== "" && status === Image.Ready
                    }

                    UiRectangle {
                        x: 4
                        y: 3
                        width: 22
                        height: 20
                        color: "#ebe4d6"
                        border.width: 1
                        border.color: "#a59882"
                        visible: modelData.occupied && !storageIcon.visible
                    }

                    UiText {
                        x: 30
                        width: parent.width - 88
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.occupied ? modelData.label : ""
                        color: "#000000"
                        font.pixelSize: 9
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        visible: modelData.occupied && !storageIcon.visible
                    }

                    UiText {
                        x: 30
                        width: parent.width - 88
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.occupied ? modelData.label : ""
                        color: "#201c16"
                        font.pixelSize: 10
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        visible: modelData.occupied && storageIcon.visible
                    }

                    UiText {
                        anchors.right: parent.right
                        anchors.rightMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.occupied ? Math.max(1, modelData.count || 0) : ""
                        color: "#5a4020"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }
            }

            UiRectangle {
                x: (uiState.storageData.scrollTrackX || 0) - uiState.storageX
                y: (uiState.storageData.scrollTrackY || 0) - uiState.storageY - 17
                width: uiState.storageData.scrollTrackWidth || 0
                height: uiState.storageData.scrollTrackHeight || 0
                visible: uiState.storageData.scrollBarVisible || false
                color: "#e8dfd3"
                border.width: 1
                border.color: "#aea08b"

                UiRectangle {
                    x: (uiState.storageData.scrollThumbX || 0) - (uiState.storageData.scrollTrackX || 0)
                    y: (uiState.storageData.scrollThumbY || 0) - (uiState.storageData.scrollTrackY || 0)
                    width: uiState.storageData.scrollThumbWidth || 0
                    height: uiState.storageData.scrollThumbHeight || 0
                    color: "#b99f74"
                    border.width: 1
                    border.color: "#755a33"
                }
            }

            UiRectangle {
                x: 0
                y: parent.height - height
                width: parent.width
                height: 21
                color: "#ddd3c4"
                border.width: 1
                border.color: "#bcaf9b"

                UiText {
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: (uiState.storageData.currentItemCount || 0) + " / " + (uiState.storageData.maxItemCount || 0)
                    color: "#5a5144"
                    font.pixelSize: 10
                }
            }
        }
    }

    UiRectangle {
        x: uiState.equipX
        y: uiState.equipY
        width: uiState.equipWidth
        height: uiState.equipHeight
        color: "#ede9df"
        border.width: 1
        border.color: "#6b675f"
        visible: uiState.equipVisible

        UiRectangle {
            x: 1
            y: 1
            width: parent.width - 2
            height: 16
            color: "#6e8194"
            border.width: 1
            border.color: "#4e5d6c"
        }

        UiText {
            x: 17
            y: 3
            text: uiState.equipData.title || ""
            color: "#ffffff"
            font.pixelSize: 12
            font.bold: true
        }

        Repeater {
            model: uiState.equipData.systemButtons || []

            delegate: UiRectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.equipX
                y: (modelData.y || 0) - uiState.equipY
                width: modelData.width || 0
                height: modelData.height || 0
                color: "#d7d2c5"
                border.width: 1
                border.color: "#7f7a70"
                visible: modelData.visible || false

                UiText {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#000000"
                    font.pixelSize: 8
                    font.bold: true
                }
            }
        }

        UiRectangle {
            x: 0
            y: 17
            width: parent.width
            height: parent.height - 17
            color: "#e7e2d6"
            visible: !uiState.equipMini

            UiRectangle {
                x: 98
                y: 15
                width: Math.max(1, 182 - 98)
                height: Math.max(1, parent.height - y - 12)
                color: "#d8d0c2"
                border.width: 1
                border.color: "#9d9488"

                Image {
                    id: equipPreviewImage
                    anchors.centerIn: parent
                    readonly property real availableWidth: Math.max(1, parent.width - 6)
                    readonly property real availableHeight: Math.max(1, parent.height - 6)
                    readonly property real sourceAspectRatio: (status === Image.Ready && sourceSize.height > 0)
                        ? (sourceSize.width / sourceSize.height)
                        : 1.0
                    width: Math.min(availableWidth, availableHeight * sourceAspectRatio)
                    height: Math.min(availableHeight, availableWidth / Math.max(0.001, sourceAspectRatio))
                    fillMode: Image.PreserveAspectFit
                    smooth: false
                    cache: false
                    source: parent.visible ? root.equipPreviewSource() : ""
                }
            }

            Repeater {
                model: uiState.equipData.slots || []

                delegate: Item {
                    required property var modelData
                    x: modelData.x - uiState.equipX
                    y: modelData.y - uiState.equipY - 17
                    width: modelData.leftColumn ? (modelData.width + 90) : (modelData.width + 60)
                    height: modelData.height

                    UiRectangle {
                        x: modelData.leftColumn ? 0 : width - modelData.width
                        y: 0
                        width: modelData.width
                        height: modelData.height
                        color: modelData.hovered ? "#d7dff0" : (modelData.occupied ? "#d7dff0" : "#f5f2ea")
                        border.width: 1
                        border.color: modelData.hovered ? "#7e95bf" : (modelData.occupied ? "#7e95bf" : "#a69f91")

                        Image {
                            id: equipIcon
                            anchors.centerIn: parent
                            width: Math.max(1, parent.width - 4)
                            height: Math.max(1, parent.height - 4)
                            fillMode: Image.PreserveAspectFit
                            smooth: false
                            cache: false
                            source: modelData.occupied && (modelData.itemId || 0) > 0 ? root.itemIconSource(modelData.itemId || 0) : ""
                            visible: source !== "" && status === Image.Ready
                        }
                    }

                    UiText {
                        x: modelData.leftColumn ? (modelData.width + 4) : 0
                        y: 0
                        width: width - modelData.width - 4
                        height: parent.height
                        text: modelData.occupied ? modelData.label : ""
                        color: "#000000"
                        font.pixelSize: 10
                        font.bold: modelData.hovered
                        horizontalAlignment: modelData.leftColumn ? Text.AlignLeft : Text.AlignRight
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        visible: modelData.occupied
                    }
                }
            }
        }
    }

    UiRectangle {
        x: uiState.friendPartyX
        y: uiState.friendPartyY
        width: uiState.friendPartyWidth
        height: uiState.friendPartyHeight
        color: "#e9e1d2"
        border.width: 1
        border.color: "#5c5243"
        visible: uiState.friendPartyVisible

        UiRectangle {
            x: 1
            y: 1
            width: parent.width - 2
            height: 16
            color: "#5c6f60"
            border.width: 1
            border.color: "#3c4a3e"
        }

        UiText {
            x: 10
            y: 2
            text: uiState.friendPartyData.title || "Friend / Party"
            color: "#f8f8f0"
            font.pixelSize: 12
            font.bold: true
        }

        Repeater {
            model: uiState.friendPartyData.systemButtons || []

            delegate: UiRectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.friendPartyX
                y: (modelData.y || 0) - uiState.friendPartyY
                width: modelData.width || 0
                height: modelData.height || 0
                color: "#d2c6b4"
                border.width: 1
                border.color: "#6e604a"
                visible: modelData.visible || false

                UiText {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#222222"
                    font.pixelSize: 9
                    font.bold: true
                }
            }
        }

        UiRectangle {
            x: 0
            y: 17
            width: parent.width
            height: parent.height - 17
            color: "#f5f0e5"

            Repeater {
                model: uiState.friendPartyData.tabs || []

                delegate: UiRectangle {
                    required property var modelData
                    x: (modelData.x || 0) - uiState.friendPartyX
                    y: (modelData.y || 0) - uiState.friendPartyY - 17
                    width: modelData.width || 0
                    height: modelData.height || 0
                    color: modelData.active ? "#fff7e9" : "#d6ccb8"
                    border.width: 1
                    border.color: modelData.active ? "#7a623e" : "#978972"

                    UiText {
                        anchors.centerIn: parent
                        text: modelData.label || ""
                        color: "#2a2a2a"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }
            }

            UiRectangle {
                x: 10
                y: 23
                width: parent.width - 26
                height: 18
                color: "#e0d8c6"
                border.width: 1
                border.color: "#ab9e88"

                UiText {
                    x: 6
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Name"
                    color: "#4c3d25"
                    font.pixelSize: 10
                    font.bold: true
                }

                UiText {
                    x: 110
                    anchors.verticalCenter: parent.verticalCenter
                    text: (uiState.friendPartyData.currentTab || 0) === 0 ? "State" : "Role"
                    color: "#4c3d25"
                    font.pixelSize: 10
                    font.bold: true
                }

                UiText {
                    x: 171
                    anchors.verticalCenter: parent.verticalCenter
                    text: (uiState.friendPartyData.currentTab || 0) === 0 ? "Memo" : "Map"
                    color: "#4c3d25"
                    font.pixelSize: 10
                    font.bold: true
                }
            }

            UiRectangle {
                x: 10
                y: 41
                width: parent.width - 26
                height: parent.height - 125
                color: "#fcf9f2"
                border.width: 1
                border.color: "#9e937c"

                Repeater {
                    model: uiState.friendPartyData.rows || []

                    delegate: UiRectangle {
                        required property var modelData
                        x: (modelData.x || 0) - uiState.friendPartyX - 10
                        y: (modelData.y || 0) - uiState.friendPartyY - 58
                        width: modelData.width || 0
                        height: modelData.height || 0
                        color: modelData.selected ? "#ebe2cd" : "#faf7ef"

                        UiRectangle {
                            x: 3
                            y: 3
                            width: 6
                            height: parent.height - 6
                            color: Qt.rgba(((modelData.color || 0) >> 16 & 255) / 255,
                                           ((modelData.color || 0) >> 8 & 255) / 255,
                                           ((modelData.color || 0) & 255) / 255,
                                           1.0)
                        }

                        UiText {
                            x: 14
                            width: 88
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.name || ""
                            color: "#1a1a1a"
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }

                        UiText {
                            x: 109
                            width: 50
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.status || ""
                            color: "#424242"
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }

                        UiText {
                            x: 171
                            width: parent.width - 175
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.detail || ""
                            color: "#5c5040"
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }
                    }
                }

                UiRectangle {
                    x: width - 12
                    y: 1
                    width: 11
                    height: parent.height - 2
                    color: "#e6dfd0"
                    border.width: 1
                    border.color: "#a8997e"
                    visible: uiState.friendPartyData.scrollBarVisible || false

                    UiRectangle {
                        x: 1
                        y: (uiState.friendPartyData.scrollThumbY || 0) - (uiState.friendPartyData.scrollTrackY || 0)
                        width: parent.width - 2
                        height: uiState.friendPartyData.scrollThumbHeight || 0
                        color: "#97835e"
                        border.width: 1
                        border.color: "#68563a"
                    }
                }
            }

            Repeater {
                model: uiState.friendPartyData.actionButtons || []

                delegate: UiRectangle {
                    required property var modelData
                    x: (modelData.x || 0) - uiState.friendPartyX
                    y: (modelData.y || 0) - uiState.friendPartyY - 17
                    width: modelData.width || 0
                    height: modelData.height || 0
                    color: (modelData.active || false) ? "#d4e2ef" : "#ebe5d6"
                    border.width: 1
                    border.color: "#796a4f"
                    visible: modelData.visible || false

                    UiText {
                        anchors.centerIn: parent
                        text: modelData.label || ""
                        color: "#1c1c1c"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }
            }

            UiRectangle {
                x: 10
                y: parent.height - 43
                width: parent.width - 20
                height: 34
                color: "#e8e2d5"
                border.width: 1
                border.color: "#b1a590"

                UiText {
                    x: 6
                    y: 4
                    text: uiState.friendPartyData.summaryTitle || ""
                    color: "#443a2c"
                    font.pixelSize: 10
                    font.bold: true
                }

                UiText {
                    x: 86
                    width: parent.width - 92
                    y: 4
                    text: uiState.friendPartyData.summaryValue || ""
                    color: "#303030"
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }

                UiText {
                    x: 6
                    y: 18
                    width: parent.width - 12
                    text: uiState.friendPartyData.summarySecondaryValue || ""
                    color: "#303030"
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }
            }
        }
    }

    UiRectangle {
        x: uiState.partySetupX
        y: uiState.partySetupY
        width: uiState.partySetupWidth
        height: uiState.partySetupHeight
        color: "#e9e1d2"
        border.width: 1
        border.color: "#5c5243"
        visible: uiState.partySetupVisible

        UiRectangle {
            x: 1
            y: 1
            width: parent.width - 2
            height: 16
            color: "#5c6f60"
            border.width: 1
            border.color: "#3c4a3e"
        }

        UiText {
            x: 10
            y: 2
            text: uiState.partySetupData.title || "Party Setup"
            color: "#f8f8f0"
            font.pixelSize: 12
            font.bold: true
        }

        Repeater {
            model: uiState.partySetupData.labels || []

            delegate: UiText {
                required property var modelData
                x: (modelData.x || 0) - uiState.partySetupX
                y: (modelData.y || 0) - uiState.partySetupY
                width: modelData.width || 0
                height: modelData.height || 0
                text: modelData.text || ""
                color: (modelData.header || false) ? "#4a3c26" : "#2e2e2e"
                font.pixelSize: 10
                font.bold: modelData.header || false
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }

        Repeater {
            model: uiState.partySetupData.helpIcons || []

            delegate: UiRectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.partySetupX
                y: (modelData.y || 0) - uiState.partySetupY
                width: modelData.width || 0
                height: modelData.height || 0
                radius: width / 2
                color: (modelData.hovered || false) ? "#d4e2ef" : "#ebe5d6"
                border.width: 1
                border.color: "#796a4f"

                UiText {
                    anchors.centerIn: parent
                    text: "?"
                    color: "#1c1c1c"
                    font.pixelSize: 10
                    font.bold: true
                }
            }
        }

        UiRectangle {
            x: (uiState.partySetupData.nameFieldX || 0) - uiState.partySetupX
            y: (uiState.partySetupData.nameFieldY || 0) - uiState.partySetupY
            width: uiState.partySetupData.nameFieldWidth || 0
            height: uiState.partySetupData.nameFieldHeight || 0
            color: (uiState.partySetupData.showNameEdit || false)
                ? ((uiState.partySetupData.nameFocused || false) ? "#ffffc8" : "#f2f2f2")
                : "transparent"
            border.width: (uiState.partySetupData.showNameEdit || false) ? 1 : 0
            border.color: "#000000"

            Text {
                font.family: (uiState.partySetupData.showNameEdit || false) ? Qt.application.font.family : uiState.uiTheme.uiFontFamily
                x: (uiState.partySetupData.showNameEdit || false) ? 6 : 0
                width: parent.width - ((uiState.partySetupData.showNameEdit || false) ? 12 : 0)
                anchors.verticalCenter: parent.verticalCenter
                text: uiState.partySetupData.nameValue || ""
                color: "#202020"
                font.pixelSize: 11
                elide: Text.ElideRight
            }
        }

        Repeater {
            model: uiState.partySetupData.choices || []

            delegate: Item {
                required property var modelData
                x: (modelData.x || 0) - uiState.partySetupX
                y: (modelData.y || 0) - uiState.partySetupY
                width: modelData.width || 0
                height: modelData.height || 0

                UiRectangle {
                    x: 0
                    y: 2
                    width: 14
                    height: 14
                    color: "#f7f3e9"
                    border.width: 1
                    border.color: "#7e6f55"

                    UiRectangle {
                        x: 3
                        y: 3
                        width: 8
                        height: 8
                        color: "#576f8a"
                        visible: modelData.selected || false
                    }
                }

                UiText {
                    x: 20
                    width: parent.width - 20
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.label || ""
                    color: "#2c2c2c"
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }
            }
        }

        Repeater {
            model: uiState.partySetupData.buttons || []

            delegate: UiRectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.partySetupX
                y: (modelData.y || 0) - uiState.partySetupY
                width: modelData.width || 0
                height: modelData.height || 0
                color: (modelData.active || false) ? "#d4e2ef" : "#ebe5d6"
                border.width: 1
                border.color: "#796a4f"
                visible: modelData.visible || false

                UiText {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#1c1c1c"
                    font.pixelSize: 10
                    font.bold: true
                }
            }
        }

        UiRectangle {
            visible: !!(uiState.partySetupData.tooltipText || "")
            x: Math.max(4, Math.min(parent.width - width - 4, ((uiState.partySetupData.tooltipX || 0) - uiState.partySetupX) - Math.floor(width / 2)))
            y: Math.max(4, Math.min(parent.height - height - 4, ((uiState.partySetupData.tooltipY || 0) - uiState.partySetupY) - height - 14))
            width: 248
            height: tooltipText.paintedHeight + 12
            color: "#303030"
            border.width: 1
            border.color: "#606060"
            z: 10

            UiText {
                id: tooltipText
                x: 8
                y: 6
                width: parent.width - 16
                text: uiState.partySetupData.tooltipText || ""
                color: "#ffffff"
                font.pixelSize: 10
                wrapMode: Text.WordWrap
            }
        }
    }

    UiRectangle {
        x: uiState.partyInviteX
        y: uiState.partyInviteY
        width: uiState.partyInviteWidth
        height: uiState.partyInviteHeight
        color: "#e9e1d2"
        border.width: 1
        border.color: "#5c5243"
        visible: uiState.partyInviteVisible

        UiRectangle {
            x: 1
            y: 1
            width: parent.width - 2
            height: 16
            color: "#5c6f60"
            border.width: 1
            border.color: "#3c4a3e"
        }

        UiRectangle {
            x: 14
            y: 28
            width: parent.width - 28
            height: 52
            color: "#efe8db"
            border.width: 1
            border.color: "#a5967d"
        }

        UiText {
            x: 10
            y: 2
            text: uiState.partyInviteData.title || "Party Invitation"
            color: "#f8f8f0"
            font.pixelSize: 12
            font.bold: true
        }

        UiText {
            x: 20
            y: 34
            width: parent.width - 40
            height: 42
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WordWrap
            text: uiState.partyInviteData.bodyText || ((uiState.partyInviteData.messageLines || []).join("\n"))
            color: "#383838"
            font.pixelSize: 11
        }

        Repeater {
            model: uiState.partyInviteData.buttons || []

            delegate: UiRectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.partyInviteX
                y: (modelData.y || 0) - uiState.partyInviteY
                width: modelData.width || 0
                height: modelData.height || 0
                color: (modelData.active || false) ? "#d4e2ef" : "#ebe5d6"
                border.width: 1
                border.color: "#796a4f"

                UiText {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#1c1c1c"
                    font.pixelSize: 10
                    font.bold: true
                }
            }
        }
    }

    UiRectangle {
        x: uiState.skillListX
        y: uiState.skillListY
        width: uiState.skillListWidth
        height: uiState.skillListHeight
        color: "#ede9df"
        border.width: 1
        border.color: "#6b675f"
        visible: uiState.skillListVisible

        UiRectangle {
            x: 1
            y: 1
            width: parent.width - 2
            height: 16
            color: "#6e8194"
            border.width: 1
            border.color: "#4e5d6c"
        }

        UiText {
            x: 17
            y: 3
            text: uiState.skillListData.title || ""
            color: "#ffffff"
            font.pixelSize: 12
            font.bold: true
        }

        Repeater {
            model: uiState.skillListData.systemButtons || []

            delegate: UiRectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.skillListX
                y: (modelData.y || 0) - uiState.skillListY
                width: modelData.width || 0
                height: modelData.height || 0
                color: "#d7d2c5"
                border.width: 1
                border.color: "#7f7a70"
                visible: modelData.visible || false

                UiText {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#000000"
                    font.pixelSize: 8
                    font.bold: true
                }
            }
        }

        UiRectangle {
            x: 41
            y: 17
            width: parent.width - 41
            height: parent.height - 67
            color: "#ffffff"
            border.width: 1
            border.color: "#c4c0b5"
        }

        UiRectangle {
            x: 0
            y: 17
            width: 41
            height: parent.height - 67
            color: "#ddd7ca"
        }

        Repeater {
            model: uiState.skillListData.rows || []

            delegate: Item {
                required property var modelData
                x: modelData.x - uiState.skillListX
                y: modelData.y - uiState.skillListY
                width: modelData.width
                height: modelData.height

                UiRectangle {
                    anchors.fill: parent
                    color: modelData.selected ? "#d7dff0" : (modelData.hovered ? "#ece8de" : "transparent")
                    border.width: 1
                    border.color: modelData.selected ? "#7e95bf" : (modelData.hovered ? "#b8b1a3" : "transparent")
                }

                UiRectangle {
                    x: 4
                    y: 1
                    width: 32
                    height: 32
                    color: "#f5f2ea"
                    border.width: 1
                    border.color: "#a69f91"

                    Image {
                        anchors.fill: parent
                        anchors.margins: 1
                        fillMode: Image.PreserveAspectFit
                        smooth: false
                        source: root.skillIconSource(modelData.skillId || 0)
                        visible: (modelData.iconVisible !== false) && source !== ""
                    }
                }

                UiText {
                    x: 48
                    y: 3
                    width: parent.width - 88
                    text: modelData.name
                    color: "#000000"
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }

                UiText {
                    x: 48
                    y: 18
                    width: 80
                    text: modelData.levelText
                    color: "#000000"
                    font.pixelSize: 10
                }

                UiText {
                    x: 165
                    y: 18
                    width: parent.width - 198
                    text: modelData.rightText
                    color: "#000000"
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }

                UiRectangle {
                    x: modelData.upgradeX - modelData.x
                    y: modelData.upgradeY - modelData.y
                    width: modelData.upgradeWidth
                    height: modelData.upgradeHeight
                    visible: modelData.upgradeVisible
                    color: modelData.upgradePressed ? "#b9c7de" : "#d7dff0"
                    border.width: 1
                    border.color: "#7e95bf"

                    UiText {
                        anchors.centerIn: parent
                        text: uiState.skillListData.upgradeLabel || ""
                        color: "#000000"
                        font.pixelSize: 12
                        font.bold: true
                    }
                }
            }
        }

        UiRectangle {
            x: (uiState.skillListData.scrollTrackX || 0) - uiState.skillListX
            y: (uiState.skillListData.scrollTrackY || 0) - uiState.skillListY
            width: uiState.skillListData.scrollTrackWidth || 0
            height: uiState.skillListData.scrollTrackHeight || 0
            visible: uiState.skillListData.scrollBarVisible || false
            color: "#e3e7ee"
            border.width: 1
            border.color: "#a4adbd"

            UiRectangle {
                x: (uiState.skillListData.scrollThumbX || 0) - (uiState.skillListData.scrollTrackX || 0)
                y: (uiState.skillListData.scrollThumbY || 0) - (uiState.skillListData.scrollTrackY || 0)
                width: uiState.skillListData.scrollThumbWidth || 0
                height: uiState.skillListData.scrollThumbHeight || 0
                color: "#b4bccd"
                border.width: 1
                border.color: "#788296"
            }
        }

        UiRectangle {
            x: 0
            y: parent.height - 50
            width: parent.width
            height: 50
            color: "#ddd7ca"
            border.width: 1
            border.color: "#bcb4a7"
        }

        UiText {
            x: 13
            y: parent.height - 18
            text: uiState.skillListData.skillPointText || ""
            color: "#b09130"
            font.pixelSize: 11
            font.bold: true
        }

        Repeater {
            model: uiState.skillListData.bottomButtons || []

            delegate: UiRectangle {
                required property var modelData
                x: modelData.x - uiState.skillListX
                y: modelData.y - uiState.skillListY
                width: modelData.width
                height: modelData.height
                color: modelData.pressed ? "#b9c7de" : (modelData.hovered ? "#d7dff0" : "#e9e4d8")
                border.width: 1
                border.color: "#8c8578"

                UiText {
                    anchors.centerIn: parent
                    text: modelData.label
                    color: "#000000"
                    font.pixelSize: 10
                }
            }
        }
    }

    UiRectangle {
        x: uiState.itemInfoX
        y: uiState.itemInfoY
        width: uiState.itemInfoWidth
        height: uiState.itemInfoHeight
        color: "#ede9df"
        border.width: 1
        border.color: "#6b675f"
        visible: uiState.itemInfoVisible
        z: 40

        UiRectangle {
            x: 1
            y: 1
            width: parent.width - 2
            height: 16
            color: "#6e8194"
            border.width: 1
            border.color: "#4e5d6c"
        }

        UiText {
            x: 8
            y: 3
            text: uiState.itemInfoData.title || "Item Information"
            color: "#ffffff"
            font.pixelSize: 12
            font.bold: true
        }

        UiRectangle {
            x: (uiState.itemInfoData.closeButton.x || 0) - uiState.itemInfoX
            y: (uiState.itemInfoData.closeButton.y || 0) - uiState.itemInfoY
            width: uiState.itemInfoData.closeButton.width || 0
            height: uiState.itemInfoData.closeButton.height || 0
            color: (uiState.itemInfoData.closeButton.pressed || false) ? "#b9c7de" : ((uiState.itemInfoData.closeButton.hovered || false) ? "#d7dff0" : "#e7e2d6")
            border.width: 1
            border.color: "#7f7a70"

            UiText {
                anchors.centerIn: parent
                text: uiState.itemInfoData.closeButton.label || "X"
                color: "#000000"
                font.pixelSize: 8
                font.bold: true
            }
        }

        UiRectangle {
            x: 8
            y: 25
            width: 96
            height: 96
            color: "#f5f2ea"
            border.width: 1
            border.color: "#a69f91"

            Image {
                anchors.fill: parent
                anchors.margins: 2
                fillMode: Image.PreserveAspectFit
                smooth: false
                cache: false
                source: (uiState.itemInfoData.previewUsesCollection || false)
                    ? root.collectionImageSource(uiState.itemInfoData.itemId || 0, uiState.itemInfoData.identified)
                    : root.itemIconSource(uiState.itemInfoData.itemId || 0, uiState.itemInfoData.identified)
            }

            UiRectangle {
                x: 4
                y: 4
                width: (uiState.itemInfoData.graphicsButton.width || 0)
                height: (uiState.itemInfoData.graphicsButton.height || 0)
                visible: uiState.itemInfoData.graphicsButton.visible || false
                color: (uiState.itemInfoData.graphicsButton.pressed || false) ? "#b9c7de" : ((uiState.itemInfoData.graphicsButton.hovered || false) ? "#d7dff0" : "#e9e4d8")
                border.width: 1
                border.color: "#8c8578"

                UiText {
                    anchors.centerIn: parent
                    text: uiState.itemInfoData.graphicsButton.label || "View"
                    color: "#000000"
                    font.pixelSize: 9
                }
            }
        }

        UiText {
            x: 112
            y: 25
            width: parent.width - 120
            text: uiState.itemInfoData.name || ""
            color: "#000000"
            font.pixelSize: 11
            font.bold: true
            elide: Text.ElideRight
        }

        Column {
            id: itemInfoDetailsColumn
            x: 112
            y: 45
            width: parent.width - 120
            spacing: 2

            Repeater {
                model: uiState.itemInfoData.detailLines || []

                delegate: UiText {
                    required property var modelData
                    width: parent.width
                    text: modelData
                    color: "#444444"
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }
            }
        }

        UiText {
            x: 112
            y: itemInfoDetailsColumn.y + itemInfoDetailsColumn.height + 4
            width: parent.width - 120
            height: parent.height - y - (((uiState.itemInfoData.slots || []).length > 0) ? 56 : 20)
            text: root.roColorTextToRichText(uiState.itemInfoData.description || "")
            textFormat: Text.RichText
            color: "#111111"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            verticalAlignment: Text.AlignTop
        }

        Repeater {
            model: uiState.itemInfoData.slots || []

            delegate: UiRectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.itemInfoX
                y: (modelData.y || 0) - uiState.itemInfoY
                width: modelData.width || 0
                height: modelData.height || 0
                color: !(modelData.available || false)
                    ? "#a6a6a6"
                    : ((modelData.occupied || false) ? "#f5f2ea" : "#dfdfdf")
                border.width: 1
                border.color: (modelData.occupied || false) ? "#868176" : "#7e7e7e"

                Image {
                    anchors.fill: parent
                    anchors.margins: 2
                    fillMode: Image.PreserveAspectFit
                    smooth: false
                    cache: false
                    visible: modelData.occupied || false
                    source: root.itemIconSource(modelData.itemId || 0)
                }

                UiRectangle {
                    visible: (modelData.hovered || false) && (modelData.occupied || false) && !!modelData.tooltip
                    x: (parent.width - width) / 2
                    y: -24
                    width: tooltipText.implicitWidth + 12
                    height: tooltipText.implicitHeight + 8
                    color: "#303030"
                    border.width: 1
                    border.color: "#606060"

                    UiText {
                        id: tooltipText
                        anchors.centerIn: parent
                        text: modelData.tooltip || ""
                        color: "#ffffff"
                        font.pixelSize: 10
                    }
                }
            }
        }
    }

    UiRectangle {
        x: uiState.skillDescribeX
        y: uiState.skillDescribeY
        width: uiState.skillDescribeWidth
        height: uiState.skillDescribeHeight
        color: "#ede9df"
        border.width: 1
        border.color: "#6b675f"
        visible: uiState.skillDescribeVisible
        z: 41

        UiRectangle {
            x: 1
            y: 1
            width: parent.width - 2
            height: 16
            color: "#6e8194"
            border.width: 1
            border.color: "#4e5d6c"
        }

        UiText {
            x: 8
            y: 3
            text: uiState.skillDescribeData.title || "Skill Information"
            color: "#ffffff"
            font.pixelSize: 12
            font.bold: true
        }

        UiRectangle {
            x: (uiState.skillDescribeData.closeButton.x || 0) - uiState.skillDescribeX
            y: (uiState.skillDescribeData.closeButton.y || 0) - uiState.skillDescribeY
            width: uiState.skillDescribeData.closeButton.width || 0
            height: uiState.skillDescribeData.closeButton.height || 0
            color: (uiState.skillDescribeData.closeButton.pressed || false) ? "#b9c7de" : ((uiState.skillDescribeData.closeButton.hovered || false) ? "#d7dff0" : "#e7e2d6")
            border.width: 1
            border.color: "#7f7a70"

            UiText {
                anchors.centerIn: parent
                text: uiState.skillDescribeData.closeButton.label || "X"
                color: "#000000"
                font.pixelSize: 8
                font.bold: true
            }
        }

        UiRectangle {
            x: 8
            y: 25
            width: 52
            height: 52
            color: "#f5f2ea"
            border.width: 1
            border.color: "#a69f91"

            Image {
                anchors.fill: parent
                anchors.margins: 2
                fillMode: Image.PreserveAspectFit
                smooth: false
                cache: false
                source: root.skillIconSource(uiState.skillDescribeData.skillId || 0)
            }
        }

        UiText {
            x: 68
            y: 25
            width: parent.width - 76
            text: uiState.skillDescribeData.name || ""
            color: "#000000"
            font.pixelSize: 11
            font.bold: true
            elide: Text.ElideRight
        }

        Column {
            x: 68
            y: 45
            width: parent.width - 76
            spacing: 2

            Repeater {
                model: uiState.skillDescribeData.detailLines || []

                delegate: UiText {
                    required property var modelData
                    width: parent.width
                    text: modelData
                    color: "#444444"
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }
            }
        }

        UiRectangle {
            x: (uiState.skillDescribeData.descriptionX || 0) - uiState.skillDescribeX
            y: (uiState.skillDescribeData.descriptionY || 0) - uiState.skillDescribeY
            width: uiState.skillDescribeData.descriptionWidth || 0
            height: uiState.skillDescribeData.descriptionHeight || 0
            color: "transparent"
            clip: true

            UiText {
                x: 0
                y: -(uiState.skillDescribeData.descriptionScrollOffset || 0)
                width: parent.width
                height: Math.max(implicitHeight, uiState.skillDescribeData.descriptionContentHeight || 0)
                text: uiState.skillDescribeData.description || ""
                color: "#111111"
                font.pixelSize: 10
                wrapMode: Text.WordWrap
                verticalAlignment: Text.AlignTop
            }
        }

        UiRectangle {
            x: (uiState.skillDescribeData.descriptionScrollBar.trackX || 0) - uiState.skillDescribeX
            y: (uiState.skillDescribeData.descriptionScrollBar.trackY || 0) - uiState.skillDescribeY
            width: uiState.skillDescribeData.descriptionScrollBar.trackWidth || 0
            height: uiState.skillDescribeData.descriptionScrollBar.trackHeight || 0
            visible: uiState.skillDescribeData.descriptionScrollBar.visible || false
            color: "#e3e7ee"
            border.width: 1
            border.color: "#a4adbd"

            UiRectangle {
                x: (uiState.skillDescribeData.descriptionScrollBar.thumbX || 0) - (uiState.skillDescribeData.descriptionScrollBar.trackX || 0)
                y: (uiState.skillDescribeData.descriptionScrollBar.thumbY || 0) - (uiState.skillDescribeData.descriptionScrollBar.trackY || 0)
                width: uiState.skillDescribeData.descriptionScrollBar.thumbWidth || 0
                height: uiState.skillDescribeData.descriptionScrollBar.thumbHeight || 0
                color: "#b4bccd"
                border.width: 1
                border.color: "#788296"
            }
        }
    }

    UiRectangle {
        x: uiState.itemCollectionX
        y: uiState.itemCollectionY
        width: uiState.itemCollectionWidth
        height: uiState.itemCollectionHeight
        color: "#ede9df"
        border.width: 1
        border.color: "#6b675f"
        visible: uiState.itemCollectionVisible
        z: 42

        UiRectangle {
            x: 1
            y: 1
            width: parent.width - 2
            height: 16
            color: "#6e8194"
            border.width: 1
            border.color: "#4e5d6c"
        }

        UiText {
            x: 8
            y: 3
            text: uiState.itemCollectionData.title || "Card illustration"
            color: "#ffffff"
            font.pixelSize: 12
            font.bold: true
        }

        UiRectangle {
            x: (uiState.itemCollectionData.closeButton.x || 0) - uiState.itemCollectionX
            y: (uiState.itemCollectionData.closeButton.y || 0) - uiState.itemCollectionY
            width: uiState.itemCollectionData.closeButton.width || 0
            height: uiState.itemCollectionData.closeButton.height || 0
            color: (uiState.itemCollectionData.closeButton.pressed || false) ? "#b9c7de" : ((uiState.itemCollectionData.closeButton.hovered || false) ? "#d7dff0" : "#e7e2d6")
            border.width: 1
            border.color: "#7f7a70"

            UiText {
                anchors.centerIn: parent
                text: uiState.itemCollectionData.closeButton.label || "X"
                color: "#000000"
                font.pixelSize: 8
                font.bold: true
            }
        }

        UiRectangle {
            x: 8
            y: 25
            width: parent.width - 16
            height: parent.height - 33
            color: "#f5f2ea"
            border.width: 1
            border.color: "#a69f91"

            Image {
                anchors.fill: parent
                anchors.margins: 0
                fillMode: Image.PreserveAspectFit
                smooth: false
                cache: false
                source: (uiState.itemCollectionData.mainUsesIllust || false)
                    ? root.illustImageSource(uiState.itemCollectionData.itemId || 0)
                    : ((uiState.itemCollectionData.mainUsesCollection || false)
                        ? root.collectionImageSource(uiState.itemCollectionData.itemId || 0)
                        : root.itemIconSource(uiState.itemCollectionData.itemId || 0))
            }
        }
    }

    UiRectangle {
        x: uiState.itemIdentifyX
        y: uiState.itemIdentifyY
        width: uiState.itemIdentifyWidth
        height: uiState.itemIdentifyHeight
        color: "#ede9df"
        border.width: 1
        border.color: "#6b675f"
        visible: uiState.itemIdentifyVisible
        z: 43

        UiRectangle {
            x: 1
            y: 1
            width: parent.width - 2
            height: 16
            color: "#6e8194"
            border.width: 1
            border.color: "#4e5d6c"
        }

        UiText {
            x: 8
            y: 3
            text: uiState.itemIdentifyData.title || "Identify Item"
            color: "#ffffff"
            font.pixelSize: 12
            font.bold: true
        }

        UiRectangle {
            x: (uiState.itemIdentifyData.closeButton.x || 0) - uiState.itemIdentifyX
            y: (uiState.itemIdentifyData.closeButton.y || 0) - uiState.itemIdentifyY
            width: uiState.itemIdentifyData.closeButton.width || 0
            height: uiState.itemIdentifyData.closeButton.height || 0
            color: (uiState.itemIdentifyData.closeButton.pressed || false) ? "#b9c7de" : ((uiState.itemIdentifyData.closeButton.hovered || false) ? "#d7dff0" : "#e7e2d6")
            border.width: 1
            border.color: "#7f7a70"

            UiText {
                anchors.centerIn: parent
                text: uiState.itemIdentifyData.closeButton.label || "X"
                color: "#000000"
                font.pixelSize: 8
                font.bold: true
            }
        }

        UiRectangle {
            id: itemIdentifyList
            x: 8
            y: 27
            width: Math.max(0, parent.width - 30)
            height: 128
            color: "#ffffff"
            border.width: 1
            border.color: "#c4c0b5"
            clip: true

            UiText {
                anchors.centerIn: parent
                visible: (uiState.itemIdentifyData.rows || []).length === 0
                text: uiState.itemIdentifyData.emptyText || "No unidentified items available."
                color: "#505050"
                font.pixelSize: 10
            }

            Repeater {
                model: uiState.itemIdentifyData.rows || []

                delegate: UiRectangle {
                    required property var modelData
                    x: (modelData.x || 0) - uiState.itemIdentifyX - itemIdentifyList.x
                    y: (modelData.y || 0) - uiState.itemIdentifyY - itemIdentifyList.y
                    width: modelData.width || 0
                    height: modelData.height || 0
                    color: (modelData.selected || false) ? "#d7dff0" : ((modelData.hovered || false) ? "#ece8de" : "#f9f7f3")
                    border.width: 1
                    border.color: (modelData.selected || false) ? "#7e95bf" : "#bcb4a7"

                    UiRectangle {
                        x: 4
                        y: 4
                        width: 24
                        height: 24
                        color: "#f5f2ea"
                        border.width: 1
                        border.color: "#a69f91"

                        Image {
                            anchors.fill: parent
                            anchors.margins: 1
                            fillMode: Image.PreserveAspectFit
                            smooth: false
                            cache: false
                            source: root.itemIconSource(modelData.itemId || 0, modelData.identified)
                        }
                    }

                    UiText {
                        x: 34
                        y: 5
                        width: parent.width - 70
                        text: modelData.label || ""
                        color: "#000000"
                        font.pixelSize: 11
                        font.bold: modelData.selected || false
                        elide: Text.ElideRight
                    }

                    UiText {
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        y: 5
                        visible: (modelData.count || 0) > 1
                        text: modelData.count || 0
                        color: "#5a4020"
                        font.pixelSize: 10
                    }
                }
            }
        }

        UiRectangle {
            x: (uiState.itemIdentifyData.scrollBar.trackX || 0) - uiState.itemIdentifyX
            y: (uiState.itemIdentifyData.scrollBar.trackY || 0) - uiState.itemIdentifyY
            width: uiState.itemIdentifyData.scrollBar.trackWidth || 0
            height: uiState.itemIdentifyData.scrollBar.trackHeight || 0
            visible: uiState.itemIdentifyData.scrollBar.visible || false
            color: "#e3e7ee"
            border.width: 1
            border.color: "#a4adbd"

            UiRectangle {
                x: (uiState.itemIdentifyData.scrollBar.thumbX || 0) - (uiState.itemIdentifyData.scrollBar.trackX || 0)
                y: (uiState.itemIdentifyData.scrollBar.thumbY || 0) - (uiState.itemIdentifyData.scrollBar.trackY || 0)
                width: uiState.itemIdentifyData.scrollBar.thumbWidth || 0
                height: uiState.itemIdentifyData.scrollBar.thumbHeight || 0
                color: "#b4bccd"
                border.width: 1
                border.color: "#788296"
            }
        }

        UiRectangle {
            x: (uiState.itemIdentifyData.okButton.x || 0) - uiState.itemIdentifyX
            y: (uiState.itemIdentifyData.okButton.y || 0) - uiState.itemIdentifyY
            width: uiState.itemIdentifyData.okButton.width || 0
            height: uiState.itemIdentifyData.okButton.height || 0
            color: !(uiState.itemIdentifyData.okButton.enabled || false)
                ? "#d0d0d0"
                : ((uiState.itemIdentifyData.okButton.pressed || false) ? "#b9c7de" : ((uiState.itemIdentifyData.okButton.hovered || false) ? "#d7dff0" : "#e9e4d8"))
            border.width: 1
            border.color: "#8c8578"

            UiText {
                anchors.centerIn: parent
                text: uiState.itemIdentifyData.okButton.label || "OK"
                color: "#000000"
                font.pixelSize: 10
            }
        }

        UiRectangle {
            x: (uiState.itemIdentifyData.cancelButton.x || 0) - uiState.itemIdentifyX
            y: (uiState.itemIdentifyData.cancelButton.y || 0) - uiState.itemIdentifyY
            width: uiState.itemIdentifyData.cancelButton.width || 0
            height: uiState.itemIdentifyData.cancelButton.height || 0
            color: (uiState.itemIdentifyData.cancelButton.pressed || false) ? "#b9c7de" : ((uiState.itemIdentifyData.cancelButton.hovered || false) ? "#d7dff0" : "#e9e4d8")
            border.width: 1
            border.color: "#8c8578"

            UiText {
                anchors.centerIn: parent
                text: uiState.itemIdentifyData.cancelButton.label || "Cancel"
                color: "#000000"
                font.pixelSize: 10
            }
        }
    }

    UiRectangle {
        x: uiState.itemCompositionX
        y: uiState.itemCompositionY
        width: uiState.itemCompositionWidth
        height: uiState.itemCompositionHeight
        color: "#ede9df"
        border.width: 1
        border.color: "#6b675f"
        visible: uiState.itemCompositionVisible
        z: 43

        UiRectangle {
            x: 1
            y: 1
            width: parent.width - 2
            height: 16
            color: "#6e8194"
            border.width: 1
            border.color: "#4e5d6c"
        }

        UiText {
            x: 8
            y: 3
            text: uiState.itemCompositionData.title || "Compound Item"
            color: "#ffffff"
            font.pixelSize: 12
            font.bold: true
        }

        UiRectangle {
            x: (uiState.itemCompositionData.closeButton.x || 0) - uiState.itemCompositionX
            y: (uiState.itemCompositionData.closeButton.y || 0) - uiState.itemCompositionY
            width: uiState.itemCompositionData.closeButton.width || 0
            height: uiState.itemCompositionData.closeButton.height || 0
            color: (uiState.itemCompositionData.closeButton.pressed || false) ? "#b9c7de" : ((uiState.itemCompositionData.closeButton.hovered || false) ? "#d7dff0" : "#e7e2d6")
            border.width: 1
            border.color: "#7f7a70"

            UiText {
                anchors.centerIn: parent
                text: uiState.itemCompositionData.closeButton.label || "X"
                color: "#000000"
                font.pixelSize: 8
                font.bold: true
            }
        }

        UiRectangle {
            id: itemCompositionList
            x: 8
            y: 27
            width: Math.max(0, parent.width - 30)
            height: 128
            color: "#ffffff"
            border.width: 1
            border.color: "#c4c0b5"
            clip: true

            UiText {
                anchors.centerIn: parent
                visible: (uiState.itemCompositionData.rows || []).length === 0
                text: uiState.itemCompositionData.emptyText || "No equipment can accept this card."
                color: "#505050"
                font.pixelSize: 10
            }

            Repeater {
                model: uiState.itemCompositionData.rows || []

                delegate: UiRectangle {
                    required property var modelData
                    x: (modelData.x || 0) - uiState.itemCompositionX - itemCompositionList.x
                    y: (modelData.y || 0) - uiState.itemCompositionY - itemCompositionList.y
                    width: modelData.width || 0
                    height: modelData.height || 0
                    color: (modelData.selected || false) ? "#d7dff0" : ((modelData.hovered || false) ? "#ece8de" : "#f9f7f3")
                    border.width: 1
                    border.color: (modelData.selected || false) ? "#7e95bf" : "#bcb4a7"

                    UiRectangle {
                        x: 4
                        y: 4
                        width: 24
                        height: 24
                        color: "#f5f2ea"
                        border.width: 1
                        border.color: "#a69f91"

                        Image {
                            anchors.fill: parent
                            anchors.margins: 1
                            fillMode: Image.PreserveAspectFit
                            smooth: false
                            cache: false
                            source: root.itemIconSource(modelData.itemId || 0, modelData.identified)
                        }
                    }

                    UiText {
                        x: 34
                        y: 5
                        width: parent.width - 70
                        text: modelData.label || ""
                        color: "#000000"
                        font.pixelSize: 11
                        font.bold: modelData.selected || false
                        elide: Text.ElideRight
                    }

                    UiText {
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        y: 5
                        visible: (modelData.count || 0) > 1
                        text: modelData.count || 0
                        color: "#5a4020"
                        font.pixelSize: 10
                    }
                }
            }
        }

        UiRectangle {
            x: (uiState.itemCompositionData.scrollBar.trackX || 0) - uiState.itemCompositionX
            y: (uiState.itemCompositionData.scrollBar.trackY || 0) - uiState.itemCompositionY
            width: uiState.itemCompositionData.scrollBar.trackWidth || 0
            height: uiState.itemCompositionData.scrollBar.trackHeight || 0
            visible: uiState.itemCompositionData.scrollBar.visible || false
            color: "#e3e7ee"
            border.width: 1
            border.color: "#a4adbd"

            UiRectangle {
                x: (uiState.itemCompositionData.scrollBar.thumbX || 0) - (uiState.itemCompositionData.scrollBar.trackX || 0)
                y: (uiState.itemCompositionData.scrollBar.thumbY || 0) - (uiState.itemCompositionData.scrollBar.trackY || 0)
                width: uiState.itemCompositionData.scrollBar.thumbWidth || 0
                height: uiState.itemCompositionData.scrollBar.thumbHeight || 0
                color: "#b4bccd"
                border.width: 1
                border.color: "#788296"
            }
        }

        UiRectangle {
            x: (uiState.itemCompositionData.okButton.x || 0) - uiState.itemCompositionX
            y: (uiState.itemCompositionData.okButton.y || 0) - uiState.itemCompositionY
            width: uiState.itemCompositionData.okButton.width || 0
            height: uiState.itemCompositionData.okButton.height || 0
            color: !(uiState.itemCompositionData.okButton.enabled || false)
                ? "#d0d0d0"
                : ((uiState.itemCompositionData.okButton.pressed || false) ? "#b9c7de" : ((uiState.itemCompositionData.okButton.hovered || false) ? "#d7dff0" : "#e9e4d8"))
            border.width: 1
            border.color: "#8c8578"

            UiText {
                anchors.centerIn: parent
                text: uiState.itemCompositionData.okButton.label || "OK"
                color: "#000000"
                font.pixelSize: 10
            }
        }

        UiRectangle {
            x: (uiState.itemCompositionData.cancelButton.x || 0) - uiState.itemCompositionX
            y: (uiState.itemCompositionData.cancelButton.y || 0) - uiState.itemCompositionY
            width: uiState.itemCompositionData.cancelButton.width || 0
            height: uiState.itemCompositionData.cancelButton.height || 0
            color: (uiState.itemCompositionData.cancelButton.pressed || false) ? "#b9c7de" : ((uiState.itemCompositionData.cancelButton.hovered || false) ? "#d7dff0" : "#e9e4d8")
            border.width: 1
            border.color: "#8c8578"

            UiText {
                anchors.centerIn: parent
                text: uiState.itemCompositionData.cancelButton.label || "Cancel"
                color: "#000000"
                font.pixelSize: 10
            }
        }
    }

    UiRectangle {
        x: uiState.optionX
        y: uiState.optionY
        width: uiState.optionWidth
        height: uiState.optionHeight
        color: "#f4f7fc"
        border.width: 1
        border.color: "#394256"
        visible: uiState.optionVisible

        UiRectangle {
            x: 0
            y: 0
            width: parent.width
            height: 17
            color: "#62729e"
            border.width: 0
        }

        UiRectangle {
            x: 0
            y: 8
            width: parent.width
            height: 9
            color: "#62729e"
        }

        UiText {
            x: 17
            y: 2
            text: uiState.optionData.title || ""
            color: "#ffffff"
            font.pixelSize: 12
            font.bold: true
        }

        Repeater {
            model: uiState.optionData.systemButtons || []

            delegate: UiRectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.optionX
                y: (modelData.y || 0) - uiState.optionY
                width: modelData.width || 0
                height: modelData.height || 0
                color: "#f8faff"
                border.width: 1
                border.color: "#607096"
                visible: modelData.visible || false

                UiText {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#28375c"
                    font.pixelSize: 9
                    font.bold: true
                }
            }
        }

        UiRectangle {
            x: (uiState.optionData.contentX || 0) - uiState.optionX
            y: (uiState.optionData.contentY || 0) - uiState.optionY
            width: uiState.optionData.contentWidth || 0
            height: uiState.optionData.contentHeight || 0
            color: "#ffffff"
            border.width: 1
            border.color: "#a0abc2"
            visible: !(uiState.optionData.collapsed || false)
        }

        Repeater {
            model: uiState.optionData.tabs || []

            delegate: UiRectangle {
                required property var modelData
                x: modelData.x - uiState.optionX
                y: modelData.y - uiState.optionY
                width: modelData.width
                height: modelData.height
                visible: !(uiState.optionData.collapsed || false)
                color: modelData.active ? "#ffffff" : "#dce4f1"
                border.width: 1
                border.color: modelData.active ? "#5f7096" : "#7a88a7"

                UiText {
                    anchors.centerIn: parent
                    text: modelData.label
                    color: "#000000"
                    font.pixelSize: 11
                }
            }
        }

        Repeater {
            model: uiState.optionData.toggles || []

            delegate: Item {
                required property var modelData
                x: modelData.x - uiState.optionX
                y: modelData.y - uiState.optionY
                width: (uiState.optionData.contentWidth || 0) - 24
                height: Math.max(modelData.height, 16)
                visible: !(uiState.optionData.collapsed || false)

                UiRectangle {
                    id: optionToggleBox
                    x: 0
                    y: 0
                    width: modelData.width
                    height: modelData.height
                    color: modelData.checked ? "#cfdaf0" : "#ffffff"
                    border.width: 1
                    border.color: "#7a88a7"
                }

                UiText {
                    anchors.left: parent.left
                    anchors.leftMargin: modelData.width + 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.label
                    color: "#000000"
                    font.pixelSize: 11
                }

                UiText {
                    anchors.centerIn: optionToggleBox
                    text: modelData.checked ? "X" : ""
                    color: "#28375c"
                    font.pixelSize: 10
                    font.bold: true
                }
            }
        }

        Repeater {
            model: uiState.optionData.sliders || []

            delegate: Item {
                required property var modelData
                x: modelData.x - uiState.optionX
                y: modelData.y - uiState.optionY
                width: modelData.width
                height: modelData.height
                visible: !(uiState.optionData.collapsed || false)
                readonly property int sliderMin: modelData.minValue !== undefined ? modelData.minValue : 0
                readonly property int sliderMax: modelData.maxValue !== undefined ? Math.max(sliderMin + 1, modelData.maxValue) : 127
                readonly property real sliderRange: Math.max(1, sliderMax - sliderMin)

                UiText {
                    x: -60
                    y: -1
                    width: 56
                    text: modelData.label
                    color: "#000000"
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignRight
                }

                UiRectangle {
                    x: 0
                    y: 4
                    width: parent.width
                    height: Math.max(1, parent.height - 8)
                    color: "#b6c2db"
                    border.width: 1
                    border.color: "#6f7da0"
                }

                UiRectangle {
                    x: 4 + ((Math.max(0, parent.width - 8) * ((modelData.value || 0) - parent.sliderMin)) / parent.sliderRange) - 4
                    y: -2
                    width: 8
                    height: parent.height + 4
                    color: "#ffffff"
                    border.width: 1
                    border.color: "#576588"
                }

                UiText {
                    x: parent.width + 8
                    y: -1
                    width: 42
                    text: modelData.valueText || ""
                    color: "#000000"
                    font.pixelSize: 11
                }
            }
        }

        Repeater {
            model: uiState.optionData.graphicsRows || []

            delegate: UiRectangle {
                required property var modelData
                x: modelData.x - uiState.optionX
                y: modelData.y - uiState.optionY
                width: modelData.width
                height: modelData.height
                visible: !(uiState.optionData.collapsed || false)
                color: "#f7faff"
                border.width: 1
                border.color: "#b0bad0"

                UiText {
                    x: 8
                    y: 4
                    text: modelData.label
                    color: "#000000"
                    font.pixelSize: 11
                }

                UiText {
                    x: 110
                    y: 4
                    width: Math.max(0, parent.width - 170)
                    text: modelData.value
                    color: "#000000"
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }

                UiRectangle {
                    x: modelData.prevX - modelData.x
                    y: modelData.prevY - modelData.y
                    width: modelData.prevWidth
                    height: modelData.prevHeight
                    color: "#f8faff"
                    border.width: 1
                    border.color: "#607096"

                    UiText {
                        anchors.centerIn: parent
                        text: modelData.prevLabel || ""
                        color: "#28375c"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                UiRectangle {
                    x: modelData.nextX - modelData.x
                    y: modelData.nextY - modelData.y
                    width: modelData.nextWidth
                    height: modelData.nextHeight
                    color: "#f8faff"
                    border.width: 1
                    border.color: "#607096"

                    UiText {
                        anchors.centerIn: parent
                        text: modelData.nextLabel || ""
                        color: "#28375c"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }
            }
        }

        UiRectangle {
            id: optionRestartButton
            readonly property var restartButtonData: uiState.optionData.restartButton || ({})
            x: (restartButtonData.x || 0) - uiState.optionX
            y: (restartButtonData.y || 0) - uiState.optionY
            width: restartButtonData.width || 0
            height: restartButtonData.height || 0
            visible: restartButtonData.visible || false
            color: "#f8faff"
            border.width: 1
            border.color: "#607096"

            UiText {
                anchors.centerIn: parent
                text: optionRestartButton.restartButtonData.label || ""
                color: "#28375c"
                font.pixelSize: 10
            }
        }

        UiRectangle {
            id: optionApplyButton
            readonly property var applyButtonData: uiState.optionData.applyButton || ({})
            x: (applyButtonData.x || 0) - uiState.optionX
            y: (applyButtonData.y || 0) - uiState.optionY
            width: applyButtonData.width || 0
            height: applyButtonData.height || 0
            visible: applyButtonData.visible || false
            color: "#f8faff"
            border.width: 1
            border.color: "#607096"

            UiText {
                anchors.centerIn: parent
                text: optionApplyButton.applyButtonData.label || ""
                color: "#28375c"
                font.pixelSize: 10
            }
        }
    }

    UiRectangle {
        x: uiState.shopChoiceX
        y: uiState.shopChoiceY
        width: uiState.shopChoiceWidth
        height: uiState.shopChoiceHeight
        color: "#f4f1ea"
        border.width: 1
        border.color: "#7f7a70"
        visible: uiState.shopChoiceVisible

        UiText {
            x: 12
            y: 10
            text: uiState.shopChoiceTitle
            color: "#2a2a2a"
            font.pixelSize: 15
            font.bold: true
        }

        UiText {
            x: 14
            y: 28
            width: parent.width - 28
            text: uiState.shopChoicePrompt
            color: "#343434"
            font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
        }

        Repeater {
            model: uiState.shopChoiceButtons

            delegate: UiRectangle {
                required property var modelData
                x: modelData.x - uiState.shopChoiceX
                y: modelData.y - uiState.shopChoiceY
                width: modelData.width
                height: modelData.height
                color: modelData.pressed ? "#aab9cd" : (modelData.hot ? "#c4d2e4" : "#dcdcdc")
                border.width: 1
                border.color: "#585858"

                UiText {
                    anchors.centerIn: parent
                    text: modelData.label
                    color: "#181818"
                    font.pixelSize: 12
                }
            }
        }
    }

    UiRectangle {
        x: uiState.serverPanelX
        y: uiState.serverPanelY
        width: uiState.serverPanelWidth
        height: uiState.serverPanelHeight
        color: "#f3f0e7"
        border.width: 1
        border.color: "#787060"
        visible: uiState.serverSelectVisible

        Column {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 4

            UiText {
                text: uiState.serverPanelData.title || ""
                color: "#303030"
                font.pixelSize: 14
                font.bold: true
            }

            Repeater {
                model: uiState.serverEntryLabels.length

                delegate: UiRectangle {
                    readonly property bool isSelected: index === uiState.serverSelectedIndex
                    readonly property bool isHovered: index === uiState.serverHoverIndex
                    readonly property int statusWidth: 64
                    readonly property int labelLeft: isSelected ? 14 : 8
                    readonly property string labelText: {
                        var base = String(uiState.serverEntryLabels[index] || "")
                        if (base.length === 0) {
                            base = "Server " + (index + 1)
                        }
                        return "#" + (index + 1) + " " + base
                    }
                    readonly property string detailText: {
                        if (isSelected) {
                            return "Selected"
                        }
                        var value = String(uiState.serverEntryDetails[index] || "")
                        if (value.length > 0) {
                            return value
                        }
                        return ""
                    }

                    width: parent ? parent.width : 0
                    height: 24
                    color: isSelected
                        ? "#d6e7b8"
                        : (isHovered ? "#ecefe5" : "#faf8f2")
                    border.width: isSelected ? 2 : 1
                    border.color: isSelected ? "#5e7f2b" : "#a0a0a0"
                    clip: true

                    UiRectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: isSelected ? 8 : 0
                        color: "#6f9630"
                        visible: isSelected
                    }

                    UiText {
                        id: serverLabel
                        x: labelLeft
                        width: Math.max(1, parent.width - labelLeft - statusWidth - 12)
                        anchors.verticalCenter: parent.verticalCenter
                        text: labelText
                        color: "#181818"
                        font.pixelSize: 12
                        font.bold: isSelected
                        elide: Text.ElideRight
                        textFormat: Text.PlainText
                        z: 1
                    }

                    UiText {
                        id: serverStatus
                        x: parent.width - statusWidth - 8
                        width: statusWidth
                        anchors.verticalCenter: parent.verticalCenter
                        text: detailText
                        color: isSelected ? "#3f6111" : "#606060"
                        font.pixelSize: 12
                        font.bold: isSelected
                        textFormat: Text.PlainText
                        horizontalAlignment: Text.AlignRight
                        elide: Text.ElideRight
                        z: 1
                    }
                }
            }
        }
    }

    UiRectangle {
        x: uiState.loginPanelX
        y: uiState.loginPanelY
        width: uiState.loginPanelWidth
        height: uiState.loginPanelHeight
        color: root.uiTheme.panelBackground
        border.width: 1
        border.color: root.uiTheme.frame
        visible: uiState.loginPanelVisible

        UiText {
            x: 24
            y: 18
            height: 26
            text: uiState.loginPanelLabels.title || "Login"
            color: root.uiTheme.uiText
            font.pixelSize: 22
            font.bold: true
            verticalAlignment: Text.AlignVCenter
        }

        UiText {
            x: 24
            y: 54
            height: 18
            text: uiState.loginPanelLabels.userLabel || "User"
            color: root.uiTheme.uiText
            font.pixelSize: 14
            verticalAlignment: Text.AlignVCenter
        }

        // Field bounds match UILoginWnd's input hit areas.
        UiRectangle {
            x: 24
            y: 76
            width: 272
            height: 30
            color: root.uiTheme.controlBackgroundText
            border.width: 1
            border.color: uiState.loginPasswordFocused ? root.uiTheme.frame : root.uiTheme.accent

            Text {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                text: uiState.loginUserId + (!uiState.loginPasswordFocused && root.loginCaretVisible ? "|" : "")
                textFormat: Text.PlainText
                color: root.uiTheme.writableText
                font.pixelSize: 14
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                clip: true
            }
        }

        UiText {
            x: 24
            y: 118
            height: 18
            text: uiState.loginPanelLabels.passwordLabel || "Password"
            color: root.uiTheme.uiText
            font.pixelSize: 14
            verticalAlignment: Text.AlignVCenter
        }

        UiRectangle {
            x: 24
            y: 140
            width: 272
            height: 30
            color: root.uiTheme.controlBackgroundText
            border.width: 1
            border.color: uiState.loginPasswordFocused ? root.uiTheme.accent : root.uiTheme.frame

            Text {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                text: uiState.loginPasswordMask + (uiState.loginPasswordFocused && root.loginCaretVisible ? "|" : "")
                textFormat: Text.PlainText
                color: root.uiTheme.writableText
                font.pixelSize: 14
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                clip: true
            }
        }

        Repeater {
            model: uiState.loginButtons

            delegate: UiRectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.loginPanelX
                y: (modelData.y || 0) - uiState.loginPanelY
                width: modelData.width || 0
                height: modelData.height || 0
                color: modelData.id === 120 ? root.uiTheme.accent : root.uiTheme.controlBackground
                border.width: 1
                border.color: root.uiTheme.frame

                UiText {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: modelData.id === 120 ? root.uiTheme.controlBackground : root.uiTheme.uiText
                    font.pixelSize: 14
                    font.bold: true
                }
            }
        }
    }

    UiRectangle {
        x: uiState.charSelectPanelX
        y: uiState.charSelectPanelY
        width: uiState.charSelectPanelWidth
        height: uiState.charSelectPanelHeight
        color: "transparent"
        border.width: 0
        visible: uiState.charSelectVisible

        Image {
            anchors.fill: parent
            fillMode: Image.Stretch
            smooth: false
            cache: false
            source: parent.visible ? ("image://openmidgard/charselectpanel?rev=" + root.charSelectPanelKey()) : ""
        }

        Repeater {
            model: uiState.charSelectSlots

            delegate: Item {
                required property var modelData
                x: modelData.x - uiState.charSelectPanelX
                y: modelData.y - uiState.charSelectPanelY
                width: modelData.width
                height: modelData.height

                Image {
                    x: 0
                    y: 0
                    width: implicitWidth
                    height: implicitHeight
                    fillMode: Image.Pad
                    smooth: false
                    cache: false
                    visible: modelData.selected
                    source: visible ? "image://openmidgard/charselectslotselected" : ""
                }
            }
        }

        Repeater {
            model: uiState.charSelectSelectedDetails.fields || []

            delegate: UiText {
                required property var modelData
                x: (modelData.x || 0) - uiState.charSelectPanelX
                y: (modelData.y || 0) - uiState.charSelectPanelY
                width: modelData.width || 0
                height: modelData.height || 0
                text: modelData.text || ""
                color: "#4b4b4b"
                font.pixelSize: 11
            }
        }

        Repeater {
            model: uiState.charSelectPageButtons

            delegate: UiRectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.charSelectPanelX
                y: (modelData.y || 0) - uiState.charSelectPanelY
                width: modelData.width || 0
                height: modelData.height || 0
                visible: modelData.visible || false
                color: "transparent"
                border.width: 0

                UiText {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#5a5448"
                    font.pixelSize: 18
                    font.bold: true
                }
            }
        }

        Repeater {
            model: uiState.charSelectActionButtons

            delegate: UiRectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.charSelectPanelX
                y: (modelData.y || 0) - uiState.charSelectPanelY
                width: modelData.width || 0
                height: modelData.height || 0
                color: "#d8d0c4"
                border.width: 1
                border.color: "#6f6558"
                visible: modelData.visible || false

                UiText {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#202020"
                    font.pixelSize: 11
                    font.bold: true
                }
            }
        }
    }

    UiRectangle {
        x: uiState.makeCharPanelX
        y: uiState.makeCharPanelY
        width: uiState.makeCharPanelWidth
        height: uiState.makeCharPanelHeight
        color: "transparent"
        border.width: 0
        visible: uiState.makeCharVisible

        Image {
            anchors.fill: parent
            fillMode: Image.Stretch
            smooth: false
            cache: false
            source: parent.visible
                ? ("image://openmidgard/makecharpanel?rev="
                    + ((uiState.makeCharPanelData && uiState.makeCharPanelData.imageRevision) || 0)
                    + "&buttons=" + root.makeCharPanelButtonsKey())
                : ""
        }

        UiRectangle {
            x: 62
            y: 244
            width: 100
            height: 18
            color: "#f2f2f2"
            border.width: 1
            border.color: uiState.makeCharNameFocused ? "#6a4c34" : "#909090"
        }

        Text {
            x: 68
            y: 246
            width: 88
            text: uiState.makeCharName + (uiState.makeCharNameFocused && root.loginCaretVisible ? "|" : "")
            color: "#202020"
            font.pixelSize: 12
            elide: Text.ElideRight
        }

        Repeater {
            model: uiState.makeCharStatFields

            delegate: UiText {
                required property var modelData
                x: (modelData.x || 0) - uiState.makeCharPanelX
                y: (modelData.y || 0) - uiState.makeCharPanelY
                width: 30
                text: modelData.value !== undefined ? modelData.value : ""
                color: "#3c2414"
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
            }
        }

        Repeater {
            model: uiState.makeCharButtons

            delegate: Item {
                required property var modelData
                x: (modelData.x || 0) - uiState.makeCharPanelX
                y: (modelData.y || 0) - uiState.makeCharPanelY
                width: modelData.width || 0
                height: modelData.height || 0
            }
        }
    }
    }
}
