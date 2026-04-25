import QtQuick
import QtQuick.Controls.Basic
import LibrovaQt

Rectangle {
    id: root

    property string sortBy: preferences.preferredSortKey
    property string sortDir: preferences.preferredSortDescending ? "desc" : "asc"
    property string genreSearchText: ""
    readonly property bool hasFocusedTextInput: _search.hasTextInputFocus || _genreSearch.hasTextInputFocus

    implicitHeight: 66
    height: implicitHeight
    color: LibrovaTheme.surfaceMuted

    Rectangle {
        anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
        height: 1
        color: LibrovaTheme.border
    }

    Row {
        id: _right
        anchors {
            right: parent.right
            rightMargin: 20
            verticalCenter: parent.verticalCenter
        }
        spacing: 8

        Rectangle {
            implicitWidth: _filterRow.implicitWidth + 22
            width: Math.max(118, implicitWidth)
            height: LibrovaTheme.controlHeight
            radius: LibrovaTheme.radiusMedium
            color: _filterHover.containsMouse ? LibrovaTheme.surfaceHover : LibrovaTheme.surfaceAlt
            border.color: _hasActiveFilters() ? LibrovaTheme.accentBorder : LibrovaTheme.border
            border.width: 1

            Row {
                id: _filterRow
                anchors.centerIn: parent
                spacing: 6
                LIcon {
                    iconPath:  LibrovaIcons.filter
                    iconColor: _hasActiveFilters() ? LibrovaTheme.accent : LibrovaTheme.textSecondary
                    size:      16
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: _filterLabel()
                    font.family: LibrovaTypography.fontFamily
                    font.pixelSize: LibrovaTypography.sizeBase
                    color: _hasActiveFilters() ? LibrovaTheme.accent : LibrovaTheme.textPrimary
                    anchors.verticalCenter: parent.verticalCenter
                }
                LIcon {
                    iconPath:  LibrovaIcons.chevronDown
                    iconColor: LibrovaTheme.accent
                    size:      12
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            HoverHandler { id: _filterHover; cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: _filterPopup.open() }

            Popup {
                id: _filterPopup
                y: parent.height + 6
                width: 420
                padding: 14
                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                background: Rectangle {
                    color: LibrovaTheme.surfaceElevated
                    radius: LibrovaTheme.radiusMedium
                    border.color: LibrovaTheme.accentBorder
                    border.width: 1
                }

                Column {
                    width: parent.width
                    spacing: 12

                    Text { text: "LANGUAGES"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeXs; font.weight: LibrovaTypography.weightSemiBold; font.letterSpacing: LibrovaTypography.spacingEyebrow; color: LibrovaTheme.textMuted }
                    Flow {
                        width: parent.width
                        spacing: 8
                        Repeater {
                            model: catalogAdapter.languageFacets
                            delegate: LGenreChip {
                                text: model.value + " (" + model.count + ")"
                                checked: model.isSelected
                                onToggled: {
                                    catalogAdapter.languageFacets.setSelected(model.value, checked)
                                    _filterDebounce.restart()
                                }
                            }
                        }
                    }

                    Rectangle { width: parent.width; height: 1; color: LibrovaTheme.borderStrong }

                    Text { text: "GENRES"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeXs; font.weight: LibrovaTypography.weightSemiBold; font.letterSpacing: LibrovaTypography.spacingEyebrow; color: LibrovaTheme.textMuted }
                    LTextInput {
                        id: _genreSearch
                        width: parent.width
                        placeholderText: "Search genres..."
                        text: root.genreSearchText
                        onTextChanged: root.genreSearchText = text
                    }
                    ScrollView {
                        width: parent.width
                        height: 200
                        contentWidth: availableWidth
                        clip: true
                        Flow {
                            width: parent.width
                            spacing: 8
                            Repeater {
                                model: catalogAdapter.genreFacets
                                delegate: LGenreChip {
                                    visible: root.genreSearchText.length === 0 || model.value.toLowerCase().indexOf(root.genreSearchText.toLowerCase()) >= 0
                                    text: model.value + " (" + model.count + ")"
                                    checked: model.isSelected
                                    onToggled: {
                                        catalogAdapter.genreFacets.setSelected(model.value, checked)
                                        _filterDebounce.restart()
                                    }
                                }
                            }
                        }
                    }

                    Row {
                        spacing: 10
                        visible: _hasActiveFilters()
                        Text { text: _activeFilterCountText(); font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeSm; color: LibrovaTheme.textSecondary; verticalAlignment: Text.AlignVCenter }
                        LButton {
                            text: "Clear all"
                            variant: "secondary"
                            implicitWidth: 110
                            onClicked: {
                                catalogAdapter.languageFacets.clearSelection()
                                catalogAdapter.genreFacets.clearSelection()
                                catalogAdapter.setLanguageFilter([])
                                catalogAdapter.setGenreFilter([])
                                _search.text = ""
                                catalogAdapter.setSearchText("")
                            }
                        }
                    }
                }
            }
        }

        Rectangle { width: 1; height: 30; anchors.verticalCenter: parent.verticalCenter; color: LibrovaTheme.borderStrong }

        Rectangle {
            height: LibrovaTheme.controlHeight
            width: 168
            radius: LibrovaTheme.radiusMedium
            color: LibrovaTheme.surfaceAlt
            border.color: LibrovaTheme.border
            border.width: 1
            clip: true

            Row {
                anchors.fill: parent
                Rectangle {
                    width: 116
                    height: parent.height
                    color: _sortHover.containsMouse ? LibrovaTheme.surfaceHover : "transparent"
                    Text { anchors.centerIn: parent; text: _sortLabel(); font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeBase; color: LibrovaTheme.textPrimary }                    HoverHandler { id: _sortHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: _sortPopup.open() }
                    Popup {
                        id: _sortPopup
                        y: parent.height + 6
                        width: 140
                        padding: 4
                        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                        background: Rectangle { color: LibrovaTheme.surfaceElevated; radius: LibrovaTheme.radiusMedium; border.color: LibrovaTheme.accentBorder; border.width: 1 }
                        Column {
                            width: parent.width
                            Repeater {
                model: [{label:"Title", value:"title"}, {label:"Author", value:"author"}, {label:"Date Added", value:"date_added"}]
                                delegate: Rectangle {
                                    width: parent.width
                                    height: 36
                                    radius: LibrovaTheme.radiusSmall
                                    color: _itemHover.containsMouse ? LibrovaTheme.surfaceHover : "transparent"
                                    Text {
                                        anchors {
                                            left: parent.left
                                            leftMargin: 10
                                            verticalCenter: parent.verticalCenter
                                        }
                                        text: modelData.label
                                        font.family: LibrovaTypography.fontFamily
                                        font.pixelSize: LibrovaTypography.sizeBase
                                        color: modelData.value === root.sortBy ? LibrovaTheme.accent : LibrovaTheme.textPrimary
                                    }
                                    HoverHandler { id: _itemHover; cursorShape: Qt.PointingHandCursor }
                                    TapHandler { onTapped: { root.sortBy = modelData.value; _sortPopup.close(); _applySort() } }
                                }
                            }
                        }
                    }
                }
                Rectangle { width: 1; height: parent.height; color: LibrovaTheme.border }
                Rectangle {
                    width: 51
                    height: parent.height
                    color: _dirHover.containsMouse ? LibrovaTheme.surfaceHover : "transparent"
                    LIcon { anchors.centerIn: parent; iconPath: root.sortDir === "asc" ? LibrovaIcons.sortAsc : LibrovaIcons.sortDesc; iconColor: LibrovaTheme.textPrimary; size: 16 }
                    HoverHandler { id: _dirHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: { root.sortDir = root.sortDir === "asc" ? "desc" : "asc"; _applySort() } }
                }
            }
        }

        Rectangle {
            width: Math.min(110, implicitWidth)
            height: LibrovaTheme.controlHeight
            radius: LibrovaTheme.controlHeight / 2
            color: LibrovaTheme.accentSurface
            border.color: LibrovaTheme.border
            border.width: 1
            implicitWidth: _bookCountLabel.implicitWidth + LibrovaTheme.sp4 * 2
            Text {
                id: _bookCountLabel
                anchors.centerIn: parent
                text: _bookCountText()
                font.family: LibrovaTypography.fontFamily
                font.pixelSize: LibrovaTypography.sizeSm
                font.weight: LibrovaTypography.weightSemiBold
                color: LibrovaTheme.textPrimary
            }
        }
    }

    LTextInput {
        id: _search
        anchors {
            left: parent.left
            leftMargin: 20
            right: _right.left
            rightMargin: 16
            verticalCenter: parent.verticalCenter
        }
        height: LibrovaTheme.controlHeight
        placeholderText: "Search titles, authors, tags..."
        leftIconPath: LibrovaIcons.search
        leftIconColor: LibrovaTheme.textSecondary
        leftIconSize: 18
        onTextChanged: _searchTimer.restart()
    }

    Timer {
        id: _searchTimer
        interval: 250
        onTriggered: catalogAdapter.setSearchText(_search.text)
    }

    Timer {
        id: _filterDebounce
        interval: 250
        onTriggered: {
            catalogAdapter.setLanguageFilter(catalogAdapter.languageFacets.selectedValues())
            catalogAdapter.setGenreFilter(catalogAdapter.genreFacets.selectedValues())
        }
    }

    function _sortLabel() {
        switch (root.sortBy) {
            case "author": return "Author"
            case "date_added": return "Date added"
            default: return "Title"
        }
    }

    function _applySort() {
        preferences.preferredSortKey = root.sortBy
        preferences.preferredSortDescending = root.sortDir === "desc"
        catalogAdapter.setSortBy(root.sortBy, root.sortDir)
    }

    function _activeFilterCount() {
        return catalogAdapter.languageFacets.selectedValues().length + catalogAdapter.genreFacets.selectedValues().length
    }

    function _hasActiveFilters() {
        return _activeFilterCount() > 0
    }

    function _filterLabel() {
        const n = _activeFilterCount()
        return n === 0 ? "Filters" : "Filters · " + n
    }

    function _activeFilterCountText() {
        const n = _activeFilterCount()
        if (n === 0) return "No filters active"
        return n === 1 ? "1 filter active" : (n + " filters active")
    }

    function _bookCountText() {
        const n = catalogAdapter.totalCount
        if (n === 1) return "1 book"
        return n.toLocaleString(Qt.locale("en_US"), 'f', 0) + " books"
    }
}
