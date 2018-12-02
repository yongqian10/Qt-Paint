import QtQuick 2.0
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.2
import QtQuick.Controls.Material 2.2
import ToolBarPlugin 1.0
import Common 1.0

RowLayout {
    id: _rootShapePicker
    objectName: "HistoryActionPickerRowLayout"

    property string resourcePath: "qrc:/resources/actionicons/"

    RoundButton {
        Material.background: Material.LightGreen
        enabled: ActionManager.undoAvailable
        contentItem: ActionBox {
            source: resourcePath + "undo.svg"
        }
        onClicked: {
            ActionManager.undo();
        }
        BasicTooltip {
            text: "Undo action"
        }
    }
    RoundButton {
        Material.background: Material.LightGreen
        enabled: ActionManager.redoAvailable
        contentItem: ActionBox {
            source: resourcePath + "redo.svg"
        }
        onClicked: {
            ActionManager.redo();
        }
        BasicTooltip {
            text: "Redo action"
        }
    }
    RoundButton {
        contentItem: ActionBox {
            source: resourcePath + "clear.svg"
        }
        onClicked: {
            ActionManager.clear();
        }
        BasicTooltip {
            text: "Clear action"
        }
    }
}
