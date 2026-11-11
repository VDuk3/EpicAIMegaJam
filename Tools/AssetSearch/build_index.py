"""
build_index.py - Descarga codepawl/asset-index (Hugging Face) y crea un indice semantico
local con EmbeddingGemma 2 (servidor REST en 127.0.0.1:8766).

Herramienta de la jam (preparada antes del 12 nov; declarada en Devlog/PRE_JAM_DECLARATION.md).
No es contenido del juego: solo un buscador de assets con su licencia.

Uso (python del venv que tiene numpy + requests):
  python -I D:/MegaJam/Tools/AssetSearch/build_index.py            # descarga + embeddings
  python -I D:/MegaJam/Tools/AssetSearch/build_index.py --stats    # solo estadisticas

Salida en Tools/AssetSearch/data/:
  rows.jsonl   una fila por asset (metadatos completos)
  emb.npy      embeddings float16 (N x 256) del campo search_text, prompt "Document"
"""
import argparse, json, os, sys, time, collections
import numpy as np
import requests

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.join(HERE, "data")
ROWS = os.path.join(DATA, "rows.jsonl")
EMB = os.path.join(DATA, "emb.npy")
API = "https://datasets-server.huggingface.co/rows"
EMBED = "http://127.0.0.1:8766/embed"
DIM = 256


def download():
    # La API de filas de Hugging Face limita la tasa (429). Se va despacio, se respeta
    # Retry-After y se guarda el progreso pagina a pagina (reanudable: si se corta, se
    # vuelve a lanzar y sigue donde quedo).
    os.makedirs(DATA, exist_ok=True)
    part = ROWS + ".part"
    offset = 0
    if os.path.exists(part):
        with open(part, encoding="utf-8") as f:
            offset = sum(1 for _ in f)
        print(f"reanudando en la fila {offset}", flush=True)
    total = None
    with open(part, "a", encoding="utf-8") as f:
        while total is None or offset < total:
            for attempt in range(12):
                r = requests.get(API, params={"dataset": "codepawl/asset-index", "config": "default",
                                              "split": "train", "offset": offset, "length": 100}, timeout=60)
                if r.status_code == 200:
                    break
                wait = int(r.headers.get("Retry-After", 0) or 0) or min(120, 10 * (attempt + 1))
                print(f"  HTTP {r.status_code} en {offset}; espero {wait}s", flush=True)
                time.sleep(wait)
            r.raise_for_status()
            j = r.json()
            total = j["num_rows_total"]
            for x in j["rows"]:
                f.write(json.dumps(x["row"], ensure_ascii=False) + "\n")
            f.flush()
            offset += len(j["rows"])
            if offset % 2000 < 100:
                print(f"  {offset}/{total}", flush=True)
            time.sleep(1.0)
    os.replace(part, ROWS)
    print(f"filas: {offset} -> {ROWS}")


def load_rows():
    with open(ROWS, encoding="utf-8") as f:
        return [json.loads(l) for l in f]


def stats(rows):
    def top(key, n=15):
        c = collections.Counter((r.get(key) if not isinstance(r.get(key), list) else tuple(r.get(key))) for r in rows)
        return c.most_common(n)
    print("total:", len(rows))
    for k in ("source", "licence", "redistributable", "kind", "style", "rigged", "animated", "ai_generated"):
        print(f"\n{k}:")
        for v, n in top(k):
            print(f"  {v}: {n}")


def embed(rows):
    texts = [(r.get("search_text") or r.get("title") or "") for r in rows]
    vecs = []
    B = 128
    t0 = time.time()
    for i in range(0, len(texts), B):
        batch = [f"title: {rows[i + k].get('title') or 'none'} | text: {t}" for k, t in enumerate(texts[i:i + B])]
        r = requests.post(EMBED, json={"texts": batch, "prompt_name": "Document", "truncate_dim": DIM, "normalize": True}, timeout=300)
        r.raise_for_status()
        vecs.append(np.asarray(r.json()["embeddings"], dtype=np.float16))
        if (i // B) % 40 == 0:
            print(f"  emb {i + len(batch)}/{len(texts)} ({time.time() - t0:.0f}s)", flush=True)
    m = np.vstack(vecs)
    np.save(EMB, m)
    print(f"embeddings {m.shape} -> {EMB} en {time.time() - t0:.0f}s")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--stats", action="store_true")
    ap.add_argument("--skip-download", action="store_true")
    a = ap.parse_args()
    if a.stats:
        stats(load_rows()); sys.exit(0)
    if not a.skip_download or not os.path.exists(ROWS):
        download()
    rows = load_rows()
    stats(rows)
    embed(rows)
