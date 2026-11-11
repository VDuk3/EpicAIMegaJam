"""
search.py - Busqueda semantica de assets (codepawl/asset-index + EmbeddingGemma 2).

Uso:
  python -I D:/MegaJam/Tools/AssetSearch/search.py "roca volcanica tileable" --k 10
  python -I .../search.py "barril de madera viejo" --kind model --redistributable
  python -I .../search.py "fantasy sword" --json        # salida JSON para agentes

Filtros: --kind (material|model|hdri|...), --source, --redistributable (solo licencias
que permiten redistribuir, p. ej. CC0), --rigged, --style.
Cada resultado muestra licencia, credito y COMO obtenerlo: anotarlo en la bitacora del dia
(tabla "Assets creados") y en la lista de terceros de la entrega.
"""
import argparse, json, os, sys
import numpy as np
import requests

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.join(HERE, "data")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("query")
    ap.add_argument("--k", type=int, default=10)
    ap.add_argument("--kind")
    ap.add_argument("--source")
    ap.add_argument("--style")
    ap.add_argument("--redistributable", action="store_true")
    ap.add_argument("--rigged", action="store_true")
    ap.add_argument("--json", action="store_true")
    a = ap.parse_args()

    with open(os.path.join(DATA, "rows.jsonl"), encoding="utf-8") as f:
        rows = [json.loads(l) for l in f]
    emb = np.load(os.path.join(DATA, "emb.npy")).astype(np.float32)

    r = requests.post("http://127.0.0.1:8766/embed", json={"texts": [a.query], "prompt_name": "SearchQuery",
                                                          "truncate_dim": emb.shape[1], "normalize": True}, timeout=60)
    r.raise_for_status()
    q = np.asarray(r.json()["embeddings"][0], dtype=np.float32)
    scores = emb @ q

    mask = np.ones(len(rows), dtype=bool)
    for i, row in enumerate(rows):
        if a.kind and row.get("kind") != a.kind: mask[i] = False
        elif a.source and row.get("source") != a.source: mask[i] = False
        elif a.style and row.get("style") != a.style: mask[i] = False
        elif a.redistributable and not row.get("redistributable"): mask[i] = False
        elif a.rigged and not row.get("rigged"): mask[i] = False
    scores = np.where(mask, scores, -9)
    idx = np.argsort(-scores)[:a.k]

    out = []
    for i in idx:
        if scores[i] <= -9: break
        row = rows[i]
        get = row.get("get")
        try: get = json.loads(get) if isinstance(get, str) else get
        except Exception: pass
        out.append({"score": round(float(scores[i]), 3), "title": row.get("title"), "source": row.get("source"),
                    "kind": row.get("kind"), "licence": row.get("licence"), "redistributable": row.get("redistributable"),
                    "credit": row.get("credit"), "url": row.get("source_url"), "get": get,
                    "triangles": row.get("triangles"), "rigged": row.get("rigged"), "tags": row.get("tags")})
    if a.json:
        print(json.dumps(out, ensure_ascii=False, indent=1))
    else:
        for o in out:
            print(f"{o['score']:.3f}  [{o['source']}/{o['kind']}] {o['title']}  | {o['licence']}"
                  f"{' (redistribuible)' if o['redistributable'] else ''}  | {o['url']}")


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    main()
