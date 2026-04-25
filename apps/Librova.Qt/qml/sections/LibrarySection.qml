import QtQuick
import LibrovaQt

Rectangle {
    id: root

    color: LibrovaTheme.surface

    LibraryToolbar {
        id: _toolbar
        anchors { top: parent.top; left: parent.left; right: parent.right }
        z: 1
    }

    Item {
        id: _contentArea
        anchors {
            top: parent.top
            topMargin: _toolbar.height
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
    }

    BookDetailsPanel {
        id: _detailsPanel
        anchors { top: _contentArea.top; right: _contentArea.right; bottom: _contentArea.bottom }
        width: _selectedBook ? 360 : 0
        clip: true
        book: _selectedBook
        details: catalogAdapter.bookDetailsModel
        loading: catalogAdapter.detailsLoading
        onCloseRequested: {
            _selectedBook = null
            _selectedBookIndex = -1
            catalogAdapter.clearBookDetails()
        }
        onExportRequested: function(bookId) { root._prepareExportDialog(bookId, "", root._selectedBook ? root._selectedBook.format : "") }
        onExportAsEpubRequested: function(bookId) { root._prepareExportDialog(bookId, "epub", "epub") }
        onTrashRequested: function(bookId) { trashAdapter.moveToTrash(bookId); _selectedBook = null; _selectedBookIndex = -1 }
        onCopyTitleRequested: function(title) {
            windowsPlatform.copyToClipboard(title)
            _toast.show("Title copied", "info")
        }
    }

    Rectangle {
        id: _rightDivider
        anchors { top: _contentArea.top; bottom: _contentArea.bottom; right: _detailsPanel.left }
        width: _detailsPanel.width > 0 ? 1 : 0
        color: LibrovaTheme.border
    }

    BookGrid {
        anchors {
            top: _contentArea.top
            left: _contentArea.left
            right: _rightDivider.left
            bottom: _contentArea.bottom
        }
        selectedBookId: _selectedBook ? _selectedBook.bookId : -1
        selectedBookIndex: _selectedBookIndex
        selectedBook: _selectedBook
        onBookClicked: function(book, bookIndex) {
            root._toggleSelectedBook(book, bookIndex)
        }
        onBookContextMenu: function(book, bookIndex, sceneX, sceneY) {
            _selectedBook = book
            _selectedBookIndex = bookIndex
            catalogAdapter.loadBookDetails(book.bookId)
            _ctxMenu.bookId = book.bookId
            _ctxMenu.bookTitle = book.title
            _ctxMenu.bookFormat = book.format
            _ctxMenu.collectionIds = book.collectionIds || []
            const pt = root.mapFromItem(null, sceneX, sceneY)
            _ctxMenu.x = pt.x
            _ctxMenu.y = pt.y
            _ctxMenu.open()
        }
    }

    BookContextMenu {
        id: _ctxMenu
        parent: root
        onExportRequested: function(bookId) { root._prepareExportDialog(bookId, "", _ctxMenu.bookFormat) }
        onExportAsEpubRequested: function(bookId) { root._prepareExportDialog(bookId, "epub", "epub") }
        onTrashRequested: function(bookId) { trashAdapter.moveToTrash(bookId) }
        onAddToCollectionRequested: function(bookId, collectionId) {
            catalogAdapter.addBookToCollection(bookId, collectionId)
        }
        onRemoveFromCollectionRequested: function(bookId, collectionId) {
            catalogAdapter.removeBookFromCollection(bookId, collectionId)
        }
        onCreateCollectionForBookRequested: function(bookId) {
            _createCollectionDialog.openForBook(bookId)
        }
        onCopyTitleRequested: function(title) {
            windowsPlatform.copyToClipboard(title)
            _toast.show("Title copied", "info")
        }
    }

    LToast {
        id: _toast
        parent: root
    }

    CollectionCreateDialog {
        id: _createCollectionDialog
    }

    Connections {
        target: catalogAdapter
        function onBookCollectionMembershipChanged(bookId) {
            catalogAdapter.refresh()
            if (_selectedBook && _selectedBook.bookId === bookId)
                catalogAdapter.loadBookDetails(bookId)
        }
        function onCollectionCreateFailed(error) {
            _toast.show(error, "error")
        }
    }

    Connections {
        target: exportAdapter
        function onExported(bookId, destinationPath) {
            _toast.show("Exported to " + destinationPath, "info")
        }
        function onExportFailed(bookId, error) {
            _toast.show("Export failed: " + error, "error")
        }
    }

    Connections {
        target: trashAdapter
        function onTrashed(bookId, destination, hadOrphanedFiles) {
            catalogAdapter.refresh()
            catalogAdapter.clearBookDetails()
            if (destination === "managed_trash")
                _toast.show("Moved to managed trash (Recycle Bin unavailable)", "warn")
        }
        function onTrashFailed(bookId, error) {
            _toast.show("Could not delete: " + error, "error")
        }
    }

    property var _selectedBook: null
    property int _selectedBookIndex: -1

    function hasSelectedBook() {
        return _selectedBook !== null
    }

    function hasFocusedTextInput() {
        return _toolbar.hasFocusedTextInput
    }

    function closeSelectedBook() {
        if (!_selectedBook)
            return false

        _selectedBook = null
        _selectedBookIndex = -1
        catalogAdapter.clearBookDetails()
        return true
    }

    function moveSelectedBookToTrash() {
        if (!_selectedBook)
            return false

        const bookId = _selectedBook.bookId
        trashAdapter.moveToTrash(bookId)
        _selectedBook = null
        _selectedBookIndex = -1
        catalogAdapter.clearBookDetails()
        return true
    }

    function _toggleSelectedBook(book, bookIndex) {
        if (_selectedBook && _selectedBook.bookId === book.bookId) {
            closeSelectedBook()
            return false
        }

        _selectedBook = book
        _selectedBookIndex = bookIndex
        catalogAdapter.loadBookDetails(book.bookId)
        return true
    }

    function _prepareExportDialog(bookId, forcedFormat, sourceFormat) {
        const book = (_selectedBook && _selectedBook.bookId === bookId) ? _selectedBook : null
        const dialogFormat = forcedFormat || sourceFormat || "epub"
        let extension = dialogFormat === "fb2" ? ".fb2" : ".epub"
        let suggested = "book" + extension
        if (book) {
            let base = _sanitizeFilename(book.title)
            if (book.authors && book.authors.length > 0)
                base += " - " + _sanitizeFilename(book.authors)
            suggested = base + extension
        }
        exportAdapter.showExportDialog(bookId, suggested, forcedFormat)
    }

    function _sanitizeFilename(str) {
        return str.replace(/[\\/:*?"<>|]/g, "_").trim()
    }

    Component.onCompleted: {
        catalogAdapter.setSortBy(
            preferences.preferredSortKey,
            preferences.preferredSortDescending ? "desc" : "asc")
        catalogAdapter.refresh()
        catalogAdapter.refreshCollections()
    }
}
