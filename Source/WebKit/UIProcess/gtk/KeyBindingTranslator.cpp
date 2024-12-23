/*
 * Copyright (C) 2010, 2011 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "KeyBindingTranslator.h"

#include <WebCore/GtkVersioning.h>
#include <gdk/gdkkeysyms.h>

namespace WebKit {

static void backspaceCallback(GtkWidget* widget, KeyBindingTranslator* translator)
{
    g_signal_stop_emission_by_name(widget, "backspace");
    translator->addPendingEditorCommand("DeleteBackward");
}

static void selectAllCallback(GtkWidget* widget, gboolean select, KeyBindingTranslator* translator)
{
    g_signal_stop_emission_by_name(widget, "select-all");
    translator->addPendingEditorCommand(select ? "SelectAll" : "Unselect");
}

static void cutClipboardCallback(GtkWidget* widget, KeyBindingTranslator* translator)
{
    g_signal_stop_emission_by_name(widget, "cut-clipboard");
    translator->addPendingEditorCommand("Cut");
}

static void copyClipboardCallback(GtkWidget* widget, KeyBindingTranslator* translator)
{
    g_signal_stop_emission_by_name(widget, "copy-clipboard");
    translator->addPendingEditorCommand("Copy");
}

static void pasteClipboardCallback(GtkWidget* widget, KeyBindingTranslator* translator)
{
    g_signal_stop_emission_by_name(widget, "paste-clipboard");
    translator->addPendingEditorCommand("Paste");
}

static void toggleOverwriteCallback(GtkWidget* widget, KeyBindingTranslator* translator)
{
    g_signal_stop_emission_by_name(widget, "toggle-overwrite");
    translator->addPendingEditorCommand("OverWrite");
}

#if GTK_CHECK_VERSION(3, 24, 0)
static void insertEmojiCallback(GtkWidget* widget, KeyBindingTranslator* translator)
{
    g_signal_stop_emission_by_name(widget, "insert-emoji");
    translator->addPendingEditorCommand("GtkInsertEmoji");
}
#endif

#if !USE(GTK4)
// GTK+ will still send these signals to the web view. So we can safely stop signal
// emission without breaking accessibility.
static void popupMenuCallback(GtkWidget* widget, KeyBindingTranslator*)
{
    g_signal_stop_emission_by_name(widget, "popup-menu");
}

static void showHelpCallback(GtkWidget* widget, KeyBindingTranslator*)
{
    g_signal_stop_emission_by_name(widget, "show-help");
}
#endif

static constexpr auto gtkDeleteCommands = std::to_array<std::array<ASCIILiteral, 2>>({
    { "DeleteBackward"_s,               "DeleteForward"_s          }, // Characters
    { "DeleteWordBackward"_s,           "DeleteWordForward"_s      }, // Word ends
    { "DeleteWordBackward"_s,           "DeleteWordForward"_s      }, // Words
    { "DeleteToBeginningOfLine"_s,      "DeleteToEndOfLine"_s      }, // Lines
    { "DeleteToBeginningOfLine"_s,      "DeleteToEndOfLine"_s      }, // Line ends
    { "DeleteToBeginningOfParagraph"_s, "DeleteToEndOfParagraph"_s }, // Paragraph ends
    { "DeleteToBeginningOfParagraph"_s, "DeleteToEndOfParagraph"_s }, // Paragraphs
    { nullptr,                          nullptr                    } // Whitespace (M-\ in Emacs)
});

static void deleteFromCursorCallback(GtkWidget* widget, GtkDeleteType deleteType, gint count, KeyBindingTranslator* translator)
{
    g_signal_stop_emission_by_name(widget, "delete-from-cursor");
    int direction = count > 0 ? 1 : 0;

    if (deleteType == GTK_DELETE_WORDS) {
        if (!direction) {
            translator->addPendingEditorCommand("MoveWordForward");
            translator->addPendingEditorCommand("MoveWordBackward");
        } else {
            translator->addPendingEditorCommand("MoveWordBackward");
            translator->addPendingEditorCommand("MoveWordForward");
        }
    } else if (deleteType == GTK_DELETE_DISPLAY_LINES) {
        if (!direction)
            translator->addPendingEditorCommand("MoveToBeginningOfLine");
        else
            translator->addPendingEditorCommand("MoveToEndOfLine");
    } else if (deleteType == GTK_DELETE_PARAGRAPHS) {
        if (!direction)
            translator->addPendingEditorCommand("MoveToBeginningOfParagraph");
        else
            translator->addPendingEditorCommand("MoveToEndOfParagraph");
    }

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    const char* rawCommand = gtkDeleteCommands[deleteType][direction];
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    if (!rawCommand)
        return;

    for (int i = 0; i < std::abs(count); i++)
        translator->addPendingEditorCommand(rawCommand);
}

static constexpr auto gtkMoveCommands = std::to_array<std::array<ASCIILiteral, 4>>({
    { "MoveBackward"_s,               "MoveForward"_s,          "MoveBackwardAndModifySelection"_s,               "MoveForwardAndModifySelection"_s          }, // Forward/backward grapheme
    { "MoveLeft"_s,                   "MoveRight"_s,            "MoveBackwardAndModifySelection"_s,               "MoveForwardAndModifySelection"_s          }, // Left/right grapheme
    { "MoveWordBackward"_s,           "MoveWordForward"_s,      "MoveWordBackwardAndModifySelection"_s,           "MoveWordForwardAndModifySelection"_s      }, // Forward/backward word
    { "MoveUp"_s,                     "MoveDown"_s,             "MoveUpAndModifySelection"_s,                     "MoveDownAndModifySelection"_s             }, // Up/down line
    { "MoveToBeginningOfLine"_s,      "MoveToEndOfLine"_s,      "MoveToBeginningOfLineAndModifySelection"_s,      "MoveToEndOfLineAndModifySelection"_s      }, // Up/down line ends
    { nullptr,                        nullptr,                  "MoveParagraphBackwardAndModifySelection"_s,      "MoveParagraphForwardAndModifySelection"_s }, // Up/down paragraphs
    { "MoveToBeginningOfParagraph"_s, "MoveToEndOfParagraph"_s, "MoveToBeginningOfParagraphAndModifySelection"_s, "MoveToEndOfParagraphAndModifySelection"_s }, // Up/down paragraph ends.
    { "MovePageUp"_s,                 "MovePageDown"_s,         "MovePageUpAndModifySelection"_s,                 "MovePageDownAndModifySelection"_s         }, // Up/down page
    { "MoveToBeginningOfDocument"_s,  "MoveToEndOfDocument"_s,  "MoveToBeginningOfDocumentAndModifySelection"_s,  "MoveToEndOfDocumentAndModifySelection"_s  }, // Begin/end of buffer
    { nullptr,                        nullptr,                  nullptr,                                          nullptr                                    }, // Horizontal page movement
});

static void moveCursorCallback(GtkWidget* widget, GtkMovementStep step, gint count, gboolean extendSelection, KeyBindingTranslator* translator)
{
    g_signal_stop_emission_by_name(widget, "move-cursor");
    int direction = count > 0 ? 1 : 0;
    if (extendSelection)
        direction += 2;

    if (static_cast<unsigned>(step) >= G_N_ELEMENTS(gtkMoveCommands))
        return;

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    const char* rawCommand = gtkMoveCommands[step][direction];
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    if (!rawCommand)
        return;

    for (int i = 0; i < std::abs(count); i++)
        translator->addPendingEditorCommand(rawCommand);
}

KeyBindingTranslator::KeyBindingTranslator()
    : m_nativeWidget(gtk_text_view_new())
{
#if USE(GTK4)
    gtk_accessible_update_state(GTK_ACCESSIBLE(m_nativeWidget.get()), GTK_ACCESSIBLE_STATE_HIDDEN, TRUE, -1);
#endif
    g_signal_connect(m_nativeWidget.get(), "backspace", G_CALLBACK(backspaceCallback), this);
    g_signal_connect(m_nativeWidget.get(), "cut-clipboard", G_CALLBACK(cutClipboardCallback), this);
    g_signal_connect(m_nativeWidget.get(), "copy-clipboard", G_CALLBACK(copyClipboardCallback), this);
    g_signal_connect(m_nativeWidget.get(), "paste-clipboard", G_CALLBACK(pasteClipboardCallback), this);
    g_signal_connect(m_nativeWidget.get(), "select-all", G_CALLBACK(selectAllCallback), this);
    g_signal_connect(m_nativeWidget.get(), "move-cursor", G_CALLBACK(moveCursorCallback), this);
    g_signal_connect(m_nativeWidget.get(), "delete-from-cursor", G_CALLBACK(deleteFromCursorCallback), this);
    g_signal_connect(m_nativeWidget.get(), "toggle-overwrite", G_CALLBACK(toggleOverwriteCallback), this);
#if !USE(GTK4)
    g_signal_connect(m_nativeWidget.get(), "popup-menu", G_CALLBACK(popupMenuCallback), this);
    g_signal_connect(m_nativeWidget.get(), "show-help", G_CALLBACK(showHelpCallback), this);
#endif
#if GTK_CHECK_VERSION(3, 24, 0)
    g_signal_connect(m_nativeWidget.get(), "insert-emoji", G_CALLBACK(insertEmojiCallback), this);
#endif
}

KeyBindingTranslator::~KeyBindingTranslator()
{
    ASSERT(!m_nativeWidget);
}

struct KeyCombinationEntry {
    unsigned gdkKeyCode;
    unsigned state;
    ASCIILiteral name;
};

static constexpr auto customKeyBindings = std::to_array<const KeyCombinationEntry>({
    { GDK_KEY_b,         GDK_CONTROL_MASK,                  "ToggleBold"_s       },
    { GDK_KEY_i,         GDK_CONTROL_MASK,                  "ToggleItalic"_s     },
    { GDK_KEY_Escape,    0,                                 "Cancel"_s           },
    { GDK_KEY_greater,   GDK_CONTROL_MASK,                  "Cancel"_s           },
    { GDK_KEY_Tab,       0,                                 "InsertTab"_s        },
    { GDK_KEY_Tab,       GDK_SHIFT_MASK,                    "InsertBacktab"_s    },
    { GDK_KEY_Return,    0,                                 "InsertNewLine"_s    },
    { GDK_KEY_KP_Enter,  0,                                 "InsertNewLine"_s    },
    { GDK_KEY_ISO_Enter, 0,                                 "InsertNewLine"_s    },
    { GDK_KEY_Return,    GDK_SHIFT_MASK,                    "InsertLineBreak"_s  },
    { GDK_KEY_KP_Enter,  GDK_SHIFT_MASK,                    "InsertLineBreak"_s  },
    { GDK_KEY_ISO_Enter, GDK_SHIFT_MASK,                    "InsertLineBreak"_s  },
    { GDK_KEY_V,         GDK_CONTROL_MASK | GDK_SHIFT_MASK, "PasteAsPlainText"_s }
});

static Vector<String> handleKeyBindingsForMap(const std::span<const KeyCombinationEntry> mapping, unsigned keyval, GdkModifierType state)
{
    // For keypress events, we want charCode(), but keyCode() does that.
    unsigned mapKey = (state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK)) << 16 | keyval;
    if (!mapKey)
        return { };

    for (const auto& item : mapping) {
        if (mapKey == (item.state << 16 | item.gdkKeyCode))
            return { item.name };
    }

    return { };
}

static Vector<String> handleCustomKeyBindings(unsigned keyval, GdkModifierType state)
{
    return handleKeyBindingsForMap(std::span(customKeyBindings), keyval, state);
}

#if USE(GTK4)
Vector<String> KeyBindingTranslator::commandsForKeyEvent(GtkEventControllerKey* controller)
{
    ASSERT(m_pendingEditorCommands.isEmpty());

    gtk_event_controller_key_forward(GTK_EVENT_CONTROLLER_KEY(controller), m_nativeWidget.get());
    if (!m_pendingEditorCommands.isEmpty())
        return WTFMove(m_pendingEditorCommands);

    auto* event = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));
    return handleCustomKeyBindings(gdk_key_event_get_keyval(event), gdk_event_get_modifier_state(event));
}
#else
Vector<String> KeyBindingTranslator::commandsForKeyEvent(GdkEventKey* event)
{
    ASSERT(m_pendingEditorCommands.isEmpty());

    gtk_bindings_activate_event(G_OBJECT(m_nativeWidget.get()), event);
    if (!m_pendingEditorCommands.isEmpty())
        return WTFMove(m_pendingEditorCommands);

    guint keyval;
    gdk_event_get_keyval(reinterpret_cast<GdkEvent*>(event), &keyval);
    GdkModifierType state;
    gdk_event_get_state(reinterpret_cast<GdkEvent*>(event), &state);
    return handleCustomKeyBindings(keyval, state);
}
#endif

static constexpr auto predefinedKeyBindings = std::to_array<const KeyCombinationEntry>({
    { GDK_KEY_Left,         0,                                 "MoveLeft"_s },
    { GDK_KEY_KP_Left,      0,                                 "MoveLeft"_s },
    { GDK_KEY_Left,         GDK_SHIFT_MASK,                    "MoveBackwardAndModifySelection"_s },
    { GDK_KEY_KP_Left,      GDK_SHIFT_MASK,                    "MoveBackwardAndModifySelection"_s },
    { GDK_KEY_Left,         GDK_CONTROL_MASK,                  "MoveWordBackward"_s },
    { GDK_KEY_KP_Left,      GDK_CONTROL_MASK,                  "MoveWordBackward"_s },
    { GDK_KEY_Left,         GDK_CONTROL_MASK | GDK_SHIFT_MASK, "MoveWordBackwardAndModifySelection"_s },
    { GDK_KEY_KP_Left,      GDK_CONTROL_MASK | GDK_SHIFT_MASK, "MoveWordBackwardAndModifySelectio"_s },
    { GDK_KEY_Right,        0,                                 "MoveRight"_s },
    { GDK_KEY_KP_Right,     0,                                 "MoveRight"_s },
    { GDK_KEY_Right,        GDK_SHIFT_MASK,                    "MoveForwardAndModifySelection"_s },
    { GDK_KEY_KP_Right,     GDK_SHIFT_MASK,                    "MoveForwardAndModifySelection"_s },
    { GDK_KEY_Right,        GDK_CONTROL_MASK,                  "MoveWordForward"_s },
    { GDK_KEY_KP_Right,     GDK_CONTROL_MASK,                  "MoveWordForward"_s },
    { GDK_KEY_Right,        GDK_CONTROL_MASK | GDK_SHIFT_MASK, "MoveWordForwardAndModifySelection"_s },
    { GDK_KEY_KP_Right,     GDK_CONTROL_MASK | GDK_SHIFT_MASK, "MoveWordForwardAndModifySelection"_s },
    { GDK_KEY_Up,           0,                                 "MoveUp"_s },
    { GDK_KEY_KP_Up,        0,                                 "MoveUp"_s },
    { GDK_KEY_Up,           GDK_SHIFT_MASK,                    "MoveUpAndModifySelection"_s },
    { GDK_KEY_KP_Up,        GDK_SHIFT_MASK,                    "MoveUpAndModifySelection"_s },
    { GDK_KEY_Down,         0,                                 "MoveDown"_s },
    { GDK_KEY_KP_Down,      0,                                 "MoveDown"_s },
    { GDK_KEY_Down,         GDK_SHIFT_MASK,                    "MoveDownAndModifySelection"_s },
    { GDK_KEY_KP_Down,      GDK_SHIFT_MASK,                    "MoveDownAndModifySelection"_s },
    { GDK_KEY_Home,         0,                                 "MoveToBeginningOfLine"_s },
    { GDK_KEY_KP_Home,      0,                                 "MoveToBeginningOfLine"_s },
    { GDK_KEY_Home,         GDK_SHIFT_MASK,                    "MoveToBeginningOfLineAndModifySelection"_s },
    { GDK_KEY_KP_Home,      GDK_SHIFT_MASK,                    "MoveToBeginningOfLineAndModifySelection"_s },
    { GDK_KEY_Home,         GDK_CONTROL_MASK,                  "MoveToBeginningOfDocument"_s },
    { GDK_KEY_KP_Home,      GDK_CONTROL_MASK,                  "MoveToBeginningOfDocument"_s },
    { GDK_KEY_Home,         GDK_CONTROL_MASK | GDK_SHIFT_MASK, "MoveToBeginningOfDocumentAndModifySelection"_s },
    { GDK_KEY_KP_Home,      GDK_CONTROL_MASK | GDK_SHIFT_MASK, "MoveToBeginningOfDocumentAndModifySelection"_s },
    { GDK_KEY_End,          0,                                 "MoveToEndOfLine"_s },
    { GDK_KEY_KP_End,       0,                                 "MoveToEndOfLine"_s },
    { GDK_KEY_End,          GDK_SHIFT_MASK,                    "MoveToEndOfLineAndModifySelection"_s },
    { GDK_KEY_KP_End,       GDK_SHIFT_MASK,                    "MoveToEndOfLineAndModifySelection"_s },
    { GDK_KEY_End,          GDK_CONTROL_MASK,                  "MoveToEndOfDocument"_s },
    { GDK_KEY_KP_End,       GDK_CONTROL_MASK,                  "MoveToEndOfDocument"_s },
    { GDK_KEY_End,          GDK_CONTROL_MASK | GDK_SHIFT_MASK, "MoveToEndOfDocumentAndModifySelection"_s },
    { GDK_KEY_KP_End,       GDK_CONTROL_MASK | GDK_SHIFT_MASK, "MoveToEndOfDocumentAndModifySelection"_s },
    { GDK_KEY_Page_Up,      0,                                 "MovePageUp"_s },
    { GDK_KEY_KP_Page_Up,   0,                                 "MovePageUp"_s },
    { GDK_KEY_Page_Up,      GDK_SHIFT_MASK,                    "MovePageUpAndModifySelection"_s },
    { GDK_KEY_KP_Page_Up,   GDK_SHIFT_MASK,                    "MovePageUpAndModifySelection"_s },
    { GDK_KEY_Page_Down,    0,                                 "MovePageDown"_s },
    { GDK_KEY_KP_Page_Down, 0,                                 "MovePageDown"_s },
    { GDK_KEY_Page_Down,    GDK_SHIFT_MASK,                    "MovePageDownAndModifySelection"_s },
    { GDK_KEY_KP_Page_Down, GDK_SHIFT_MASK,                    "MovePageDownAndModifySelection"_s },
    { GDK_KEY_Delete,       0,                                 "DeleteForward"_s },
    { GDK_KEY_KP_Delete,    0,                                 "DeleteForward"_s },
    { GDK_KEY_Delete,       GDK_CONTROL_MASK,                  "DeleteWordForward"_s },
    { GDK_KEY_KP_Delete,    GDK_CONTROL_MASK,                  "DeleteWordForward"_s },
    { GDK_KEY_BackSpace,    0,                                 "DeleteBackward"_s },
    { GDK_KEY_BackSpace,    GDK_SHIFT_MASK,                    "DeleteBackward"_s },
    { GDK_KEY_BackSpace,    GDK_CONTROL_MASK,                  "DeleteWordBackward"_s },
    { GDK_KEY_a,            GDK_CONTROL_MASK,                  "SelectAll"_s },
    { GDK_KEY_a,            GDK_CONTROL_MASK | GDK_SHIFT_MASK, "Unselect"_s },
    { GDK_KEY_slash,        GDK_CONTROL_MASK,                  "SelectAll"_s },
    { GDK_KEY_backslash,    GDK_CONTROL_MASK,                  "Unselect"_s },
    { GDK_KEY_x,            GDK_CONTROL_MASK,                  "Cut"_s },
    { GDK_KEY_c,            GDK_CONTROL_MASK,                  "Copy"_s },
    { GDK_KEY_v,            GDK_CONTROL_MASK,                  "Paste"_s },
    { GDK_KEY_KP_Delete,    GDK_SHIFT_MASK,                    "Cut"_s },
    { GDK_KEY_KP_Insert,    GDK_CONTROL_MASK,                  "Copy"_s },
    { GDK_KEY_KP_Insert,    GDK_SHIFT_MASK,                    "Paste"_s }
});

Vector<String> KeyBindingTranslator::commandsForKeyval(unsigned keyval, unsigned modifiers)
{
    auto commands = handleKeyBindingsForMap(predefinedKeyBindings, keyval, static_cast<GdkModifierType>(modifiers));
    if (!commands.isEmpty())
        return commands;

    return handleCustomKeyBindings(keyval, static_cast<GdkModifierType>(modifiers));
}

} // namespace WebKit
