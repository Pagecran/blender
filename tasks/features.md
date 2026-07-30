# Pagecran Feature Registry

This registry tracks Pagecran changes carried on top of Blender 5.2 for future Blender ports.

## Delivered

| Feature | Source / commit | Port notes |
| :--- | :--- | :--- |
| Cycles production defaults | `32db0548da9` | GPU default, OptiX/CUDA selection, 2048 render samples, denoise, Cryptomatte, Diffuse Color, and AO passes. |
| Standard color management defaults | `d91670df4cc` | Keep Blender defaults: Linear Rec.709 working space and AgX view. Do not migrate existing blend files. |
| Shader Editor Palette | `a36f3b9a7f2` | Native shared Palette panel in Shader Editor > Color. |
| Shader Editor palette eyedropper | `a6349310c60` | Screen color picker writes to the active palette brush color. |
| NLA Toggle Mute in All View Layers | 5.2.0.16 | Adds the command to Track and NLA channel context menus. Selected tracks are globally muted, so the state applies in every View Layer. |
| Camera aim target navigation and rig | `db2214218e3` | Retain and retest when porting editor navigation changes. |
| Shader Editor color panel | `a36f3b9a7f2` | See the dedicated task record in `tasks/done/`. |

## Port Candidates

| Feature | Source | Status / port guidance |
| :--- | :--- | :--- |
| Per-View-Layer NLA mute overrides | `fb6224ba549` on `Pagec5.1.1` | Not ported. It changes DNA, blend-file IO, copy-on-write, animation channel behavior, and NLA drawing. Port only with dedicated compatibility tests. |
| Shot Manager Pro 2.0.8.6 | Workgroup commit `64ea5ac` | Maintained in `R:\Workgroup_Blender\Extension\System\shot_manager`; test independently from Blender source ports. |

## Current Work

_None._
