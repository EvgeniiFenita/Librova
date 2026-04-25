import QtQuick
import QtQuick.Window
import LibrovaQt

Window {
    id: root

    title: "Librova"
    x: shellState.windowX
    y: shellState.windowY
    width: Math.max(shellState.windowWidth, minimumWidth)
    height: Math.max(shellState.windowHeight, minimumHeight)
    minimumWidth: 1110
    minimumHeight: 840
    visible: true
    color: LibrovaTheme.background

    Component.onCompleted: {
        if (shellState.maximized)
            root.visibility = Window.Maximized
    }

    onXChanged: {
        if (visibility !== Window.Maximized)
            shellState.windowX = x
    }
    onYChanged: {
        if (visibility !== Window.Maximized)
            shellState.windowY = y
    }
    onWidthChanged: {
        if (visibility !== Window.Maximized)
            shellState.windowWidth = width
    }
    onHeightChanged: {
        if (visibility !== Window.Maximized)
            shellState.windowHeight = height
    }
    onVisibilityChanged: function() {
        shellState.maximized = root.visibility === Window.Maximized
    }

    AppShell {
        anchors.fill: parent
    }
}

