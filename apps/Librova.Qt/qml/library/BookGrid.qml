import QtQuick
import QtQuick.Controls.Basic
import LibrovaQt

// Scrollable grid of BookCard delegates.
// Reads catalogAdapter.bookListModel directly (context property).
//
// Properties:
//   selectedBookId — qint64; -1 means no selection
//
// Signals:
//   bookClicked(book)            — JS snapshot of all model roles
//   bookContextMenu(book, index, sx, sy) — scene-space position for the context popup

Item {
    id: root

    property var selectedBookId: -1
    property int selectedBookIndex: -1
    property var selectedBook: null

    signal bookClicked(var book, int bookIndex)
    signal bookContextMenu(var book, int bookIndex, real sceneX, real sceneY)

    // ── Empty state ────────────────────────────────────────────────────────────

    Item {
        id: _emptyCard
        objectName: "EmptyStateCard"
        width: Math.min(420, Math.max(280, root.width - 32))
        height: _emptyState.implicitHeight + 36
        x: (root.width - width) / 2
        y: (root.height - height) / 2
        visible: !catalogAdapter.loading && catalogAdapter.totalCount === 0

        Column {
            id: _emptyState
            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
                margins: 18
            }
            spacing: 14

            Item {
                objectName:    "EmptyStateIcon"
                width:         parent.width
                height:        160
                implicitHeight: 160

                EmptyBookIllustration {
                    anchors.centerIn: parent
                    visible: catalogAdapter.showGoToImportButton
                }

                NoResultsIllustration {
                    anchors.centerIn: parent
                    visible: !catalogAdapter.showGoToImportButton
                }
            }

            Text {
                objectName: "EmptyStateTitle"
                width: parent.width
                text: catalogAdapter.showGoToImportButton ? "Library is empty" : "Nothing found"
                font.family: LibrovaTypography.fontFamily
                font.pixelSize: LibrovaTypography.sizeMd
                font.weight: LibrovaTypography.weightSemiBold
                color: LibrovaTheme.textPrimary
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                objectName: "EmptyStateBody"
                width: parent.width
                text: catalogAdapter.showGoToImportButton
                    ? "Import your first book to start building the library."
                    : "Try a different search or clear the active filters."
                font.family: LibrovaTypography.fontFamily
                font.pixelSize: LibrovaTypography.sizeBase
                color: LibrovaTheme.textSecondary
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }

            Item {
                width: parent.width
                height: _emptyStateButton.visible ? 42 : 0

                LButton {
                    id: _emptyStateButton
                    objectName: "EmptyStateButton"
                    anchors.centerIn: parent
                    text: catalogAdapter.showGoToImportButton ? "Go to Import" : "Clear all"
                    variant: catalogAdapter.showGoToImportButton ? "accent" : "secondary"
                    visible: catalogAdapter.showGoToImportButton || catalogAdapter.showClearFiltersButton
                    onClicked: {
                        if (catalogAdapter.showGoToImportButton)
                            shellState.lastSection = "import"
                        else
                            catalogAdapter.clearAllFilters()
                    }
                }
            }
        }
    }

    // ── Initial load pulse ─────────────────────────────────────────────────────

    Rectangle {
        anchors.centerIn: parent
        width: 48; height: 48; radius: 24
        color:        LibrovaTheme.accentMuted
        border.color: LibrovaTheme.accent
        border.width: 2
        visible:      catalogAdapter.loading && catalogAdapter.totalCount === 0

        SequentialAnimation on opacity {
            loops:   Animation.Infinite
            running: parent.visible
            NumberAnimation { to: 0.3; duration: 600; easing.type: Easing.InOutSine }
            NumberAnimation { to: 1.0; duration: 600; easing.type: Easing.InOutSine }
        }
    }

    // ── Grid ───────────────────────────────────────────────────────────────────

    GridView {
        id: _grid
        anchors.fill:    parent
        anchors.margins: 16
        visible:         catalogAdapter.totalCount > 0
        clip:            true

        cellWidth:  196
        cellHeight: 324

        model: catalogAdapter.bookListModel

        ScrollBar.vertical: LScrollBar {
            policy: ScrollBar.AsNeeded
        }

        onContentYChanged: {
            const remainingHeight = contentHeight - (contentY + height)
            if (remainingHeight <= 320)
                catalogAdapter.loadMore()
        }

        footer: Item {
            width: _grid.width
            height: catalogAdapter.isLoadingMore ? 56 : 0

            Text {
                anchors.centerIn: parent
                visible: catalogAdapter.isLoadingMore
                text: "Loading more books..."
                font.family: LibrovaTypography.fontFamily
                font.pixelSize: LibrovaTypography.sizeSm
                color: LibrovaTheme.textMuted
            }
        }

        delegate: BookCard {
            width:  172
            height: 300

            book: ({
                bookId:      model.bookId,
                title:       model.title,
                authors:     model.authors,
                language:    model.language,
                seriesName:  model.seriesName,
                seriesIndex: model.seriesIndex,
                year:        model.year,
                genres:      model.genres,
                format:      model.format,
                managedPath: model.managedPath,
                coverPath:   model.coverPath,
                sizeBytes:   model.sizeBytes,
                addedAt:     model.addedAt,
                collectionIds: model.collectionIds,
                collectionNames: model.collectionNames
            })

            selected: model.bookId === root.selectedBookId

            onClicked: root.bookClicked(book, index)

            onRightClicked: function(sceneX, sceneY) {
                root.bookContextMenu(book, index, sceneX, sceneY)
            }
        }
    }

    Timer {
        id: _selectionPositionTimer
        interval: 0
        onTriggered: {
            if (root.selectedBookIndex >= 0 && root.selectedBookIndex < _grid.count)
                _grid.positionViewAtIndex(root.selectedBookIndex, GridView.Contain)
        }
    }

    onSelectedBookIndexChanged: _selectionPositionTimer.restart()
}