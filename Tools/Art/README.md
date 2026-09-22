# Procedural art source

The project includes editable FBX geometry, procedural PNG surfaces, Python authoring recipes, and the UE project assets. Normal project setup uses these checked-in assets; Blender regeneration is optional. Artwork is covered by `ASSET-LICENSE`; authoring code is covered by `LICENSE`. See the root notices for attribution and separate technical dependencies.

## Generate a selected asset family

Use Python 3 and your own Blender 4.5 installation. Work in a copy of this repository when experimenting: recipes replace that checkout's generated FBX, textures and catalogs. They can update multiple related objects. They do not launch Unreal Engine.

```powershell
python Tools/Art/generate_art.py --list
python Tools/Art/generate_art.py --recipe aero-public --blender "<your Blender installation>/blender.exe"
```

`aero-public` creates the public observatory-barge hull and stern ring drives. `aero-support` additionally regenerates the existing deck/interior/terminal assets and collision recipe within their original bounds. The public exterior no longer uses the previous paired sweeping arches or swept tail fins. Public hull/fins were regenerated with Blender 4.5.10; a clean regeneration of every historical family has not been tested.

Other named recipes cover the rail vehicles, pools, hotel rooms, water-city palaces, interior detail and roof gardens. Some older recipes optionally save `.blend` files; those caches are not distributed. `Project/SourceArt/Generated` holds local reports and must not be included in the public source archive.

Some detail recipes deliberately refuse to overwrite a previous candidate manifest. Preserve or rename that manifest in your experimental checkout before rerunning; the runner does not bypass those authoring checks. Logs are written to `SourceArt/Generated/<recipe>-blender.log`.

The original layered kit also retains lower-level scripts (`build_kit.py`, `expand_art.py`, `vertical_world.py`, `forest_world.py`, `understory_world.py`, `city_volume.py`, `city_districts.py`, `city_access_kit.py`, `rain_window_kit.py`, `water_city_kit.py`, `water_city_gardens.py`, and `walk_floor_geometry.py`). These represent sequential authoring revisions, not a single clean rebuild command. Catalogs, dependencies and runtime layouts should be reviewed when rebuilding those families. Existing supplied FBX is the authoritative import source.

`ImportedBaselines` contains 17 exact historical geometry sources recovered by matching the original UE import MD5. Retaining these avoids silently replacing reviewed geometry with later authoring revisions. A recipe-family association is not a bit-identical regeneration claim.

## UE imports

Matching `import_*.py` scripts under `Project/SourceArt/Scripts` describe material slots and asset import options. Run them inside your built Unreal Editor against a disposable project copy first. Several historical importers also adjust scene setup; they are authoring utilities rather than required setup steps.

`Tools/Art/import_public_aero.py` is the bounded release importer for `SM_Aero87Hull`, `SM_Aero87Fins`, and `SM_SkyTheatre87Deck`. It validates the current project's path and source catalog hashes, retains existing material interfaces, enables Nanite and leaves walking collision assets unchanged. The release preparation ran that importer successfully. The final deck bounds are checked against the existing mesh before import.

## Engine-derived cloud materials

Two raw assets are deliberately **excluded from the public source archive**:

- `Project/Content/EndlessWorld/Materials/M_CloudLayers.uasset`
- `Project/Content/EndlessWorld/Materials/MI_CloudSea.uasset`

`Tools/Art/ensure_cloud_materials.py` creates them only if missing, using the recipient's installed UE material `/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud` and its `_Inst` instance. It applies this project's own `Project/SourceArt/Shaders/CloudLayerMask.hlsl` and the settings in `cloud-material-recipe.json`. The setup helper must run after building the editor target; root `build.ps1` wires it into setup. Existing material assets are preserved and their parameters reported. Cooked game distribution can include these under the Unreal Engine license; the project asset license does not relicense Epic content.

Other engine material functions are referenced from the installed engine, not copied as source files. The art scripts do not contain engine source. Fonts and technical libraries are covered by the root third-party notices.

## Provenance and checks

`provenance.json` lists every Content file's source recipe and every distributed SourceArt file's SHA-256. Content hashes are assigned after UE import-path sanitization by the release packager. Unknown files fail the ledger builder. Audio is separately audited; its `delegated-audio` classification requires the release audio ledger, and does not independently authorize distribution.

```powershell
python Tools/Art/audit_fbx.py Project/SourceArt --report Project/SourceArt/Generated/fbx-audit.json
```

All 294 supplied FBX files were structurally parsed: none contains embedded image/video payload or texture-file references. Personal Blender filenames in 32 files were replaced only inside FBX SceneInfo strings, with every byte outside those metadata ranges preserved. All 69 supplied texture PNGs were associated with procedural project generators and original UE import hashes. External reference images, sampled audio, `.blend` backups and private development reports are omitted.

This is a source/provenance audit, not a legal originality guarantee or a claim that the full game was visually retested. Historical reference images influenced the original art direction but no reference pixels or files are distributed. The public Aero exterior was independently redesigned for this release.
