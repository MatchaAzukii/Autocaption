// Prevents additional console window on Windows in release
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use tauri::menu::{Menu, MenuItem};
use tauri::tray::TrayIconBuilder;
use tauri::Manager;

fn main() {
    tauri::Builder::default()
        .plugin(tauri_plugin_shell::init())
        .plugin(tauri_plugin_http::init())
        .setup(|app| {
            let handle = app.handle();
            
            // Create tray icon
            let quit_i = MenuItem::with_id(handle, "quit", "Quit", true, None::<&str>)?;
            let show_i = MenuItem::with_id(handle, "show", "Show Dashboard", true, None::<&str>)?;
            let overlay_i = MenuItem::with_id(handle, "overlay", "Toggle Overlay", true, None::<&str>)?;
            
            let menu = Menu::with_items(handle, &[&show_i, &overlay_i, &quit_i])?;
            
            let _tray = TrayIconBuilder::with_id("tray")
                .menu(&menu)
                .tooltip("Autocaption")
                .on_menu_event(|app: &tauri::AppHandle, event| match event.id.as_ref() {
                    "quit" => {
                        app.exit(0);
                    }
                    "show" => {
                        if let Some(window) = app.get_webview_window("main") {
                            let _: Result<(), _> = window.show();
                            let _: Result<(), _> = window.set_focus();
                        }
                    }
                    "overlay" => {
                        toggle_overlay(app);
                    }
                    _ => {}
                })
                .build(handle)?;

            // Create overlay window initially (hidden)
            let _ = create_overlay_window(handle);

            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            toggle_overlay_command,
            set_click_through,
            get_backend_url
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}

fn create_overlay_window(handle: &tauri::AppHandle) -> Result<tauri::WebviewWindow, tauri::Error> {
    tauri::WebviewWindowBuilder::new(
        handle,
        "overlay",
        tauri::WebviewUrl::App("index.html?mode=overlay".into())
    )
    .title("Autocaption Overlay")
    .inner_size(800.0, 120.0)
    .position(200.0, 800.0)
    .decorations(false)
    .always_on_top(true)
    .skip_taskbar(true)
    .visible(false)
    .build()
}

#[tauri::command]
fn toggle_overlay_command(handle: tauri::AppHandle) {
    toggle_overlay(&handle);
}

#[tauri::command]
fn set_click_through(window: tauri::WebviewWindow, enabled: bool) {
    let _ = window.set_ignore_cursor_events(enabled);
}

#[tauri::command]
fn get_backend_url() -> String {
    "ws://localhost:8765".to_string()
}

fn toggle_overlay(handle: &tauri::AppHandle) {
    if let Some(overlay) = handle.get_webview_window("overlay") {
        if overlay.is_visible().unwrap_or(false) {
            let _ = overlay.hide();
            if let Some(main) = handle.get_webview_window("main") {
                let _ = main.show();
            }
        } else {
            let _ = overlay.show();
            let _ = overlay.set_focus();
        }
    }
}
