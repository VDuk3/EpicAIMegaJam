# AssetSearch: buscador semántico de assets con licencia

Busca en **37.668 assets**, en español o en inglés, sobre el índice público `codepawl/asset-index` (Hugging Face), con embeddings locales de EmbeddingGemma 2. Es una herramienta preparada antes de la jam: no es contenido del juego.

| Fuente | Assets | Licencia | Cómo se obtiene |
|---|---|---|---|
| Megascans (Fab) | 22.696 | Fab Standard: se usa en el proyecto, **no** es redistribuible | "Claim" en Fab con tu cuenta, luego descargar |
| Kenney | 4.434 | CC0 | descarga directa |
| Smithsonian | 3.451 | CC0 | descarga directa |
| ambientCG | 2.917 | CC0 | descarga directa |
| Poly Haven | 2.387 | CC0 | descarga directa |
| Quaternius | 1.783 | CC0 | descarga directa |

Tipos: 19.113 modelos, 14.175 materiales, 2.358 decals, 1.432 HDRI. 192 assets están riggeados y 316 animados.

## Uso
Antes, arranca el servidor de embeddings: `D:\Models\EmbeddingGemma-2\start_embedding_server.bat` (puerto 8766).
```powershell
powershell -File D:\MegaJam\Tools\AssetSearch\search.ps1 "roca volcánica con lava" --k 10
powershell -File D:\MegaJam\Tools\AssetSearch\search.ps1 "barril de madera" --kind model
powershell -File D:\MegaJam\Tools\AssetSearch\search.ps1 "espada medieval" --redistributable
powershell -File D:\MegaJam\Tools\AssetSearch\search.ps1 "personaje para jugador" --rigged --json
```
Filtros:
- `--kind` (model, material, decal, hdri, brush, atlas, terrain);
- `--style` (photoreal, lowpoly, stylized);
- `--source`;
- `--redistributable` (solo CC0);
- `--rigged`;
- `--json` (salida para agentes).

## Reglas de la jam
- Cada asset usado va a la tabla "Assets creados" de la entrada del día (título, fuente, licencia, URL) y a la lista de terceros de la entrega.
- Megascans vía Fab: válido para un juego hecho en Unreal, pero no se redistribuye fuera del build.

## Reconstruir el índice
`python -I build_index.py` descarga el índice (por la API de filas de Hugging Face, despacio y reanudable, porque responde 429/502 si se le pide rápido) y genera `data/emb.npy` (unos 97 s en GPU).
- Datos: `data/rows.jsonl` (69 MB) y `data/emb.npy` (19 MB). Para otra máquina, copia esa carpeta y te ahorras reconstruirlo.
- El Python debe tener `numpy` y `requests`. En esta máquina: `D:\Modly\extensions\hunyuan3d-mini\venv\Scripts\python.exe`.
