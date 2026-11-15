"""Estrae dal codice le battute che i personaggi dicono, per farle doppiare.

    python extract_lines.py [--all | --day2] [--catalog file.json] [--no-hints] > voice_lines.json

Legge Source/ClaraDayOne/ClaraStoryDirector.cpp (e ClaraStoryDay2.cpp, se esiste), trova ogni chiamata Say(...)
con battute scritte per esteso e ne ricava: personaggio, testo, chiave. La chiave e' la stessa calcolata dal gioco
(ClaraAudio::VoiceKey): MD5 del testo UTF-8 senza spazi ai bordi, prime 16 cifre. Il file audio di una battuta e'
Content/Audio/Voices/<Personaggio>/<chiave>.wav. Le battute composte a runtime non si possono doppiare e vengono saltate.
"""
import hashlib
import json
import os
import re
import sys
from pathlib import Path

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SOURCES = ["Source/ClaraDayOne/ClaraStoryDirector.cpp", "Source/ClaraDayOne/ClaraStoryDay2.cpp",
           "Source/ClaraDayOne/ClaraArrivalInterview.cpp"]
VOICES = {"Luca", "Alessia", "Gaia", "Jonny"}
ESCAPES = {"n": "\n", "t": "\t", '"': '"', "'": "'", "\\": "\\"}


def key(text):
    return hashlib.md5(text.strip().encode("utf-8")).hexdigest()[:16]


def read_literal(src, i):
    """src[i] e' la virgoletta di apertura: restituisce (testo, indice dopo la chiusura)."""
    out, i = [], i + 1
    while src[i] != '"':
        if src[i] == "\\":
            out.append(ESCAPES.get(src[i + 1], src[i + 1]))
            i += 2
        else:
            out.append(src[i])
            i += 1
    return "".join(out), i + 1


def split_args(src, i):
    """src[i] e' la parentesi aperta: restituisce (argomenti al primo livello, indice dopo la chiusura)."""
    close = {"(": ")", "{": "}", "[": "]"}
    stack, args, start, j = [src[i]], [], i + 1, i + 1
    while stack:
        c = src[j]
        if c == '"':
            _, j = read_literal(src, j)
            continue
        if c in close:
            stack.append(c)
        elif c in ")}]":
            stack.pop()
            if not stack:
                args.append(src[start:j])
        elif c == "," and len(stack) == 1:
            args.append(src[start:j])
            start = j + 1
        j += 1
    return [a.strip() for a in args], j


def literals(expr):
    """Battute scritte per esteso: TEXT("...") da solo, oppure una scelta fra due TEXT("...")."""
    found, i = [], 0
    if "FString::Printf" in expr or "+" in re.sub(r'"(\\.|[^"\\])*"', '""', expr):
        return []  # testo composto a runtime
    while True:
        m = re.search(r'TEXT\(\s*"', expr[i:])
        if not m:
            return found
        text, i = read_literal(expr, i + m.end() - 1)
        found.append(text)


def interview_variants(src):
    """Il conteggio del colloquio ha quattro esiti; il nome libero resta solo testo."""
    for match in re.finditer(r"FString::Printf\(", src):
        args, _ = split_args(src, match.end() - 1)
        if len(args) != 4 or args[1] != "ArrivalNetworkingAnswers":
            continue
        maximum = re.fullmatch(r"(\d+)\s*-\s*ArrivalNetworkingAnswers", args[2])
        threshold = re.match(r"ArrivalNetworkingAnswers\s*>=\s*(\d+)\s*\?", args[3])
        formats, careers = literals(args[0]), literals(args[3])
        if not maximum or not threshold or len(formats) != 1 or len(careers) != 2:
            raise ValueError("Riepilogo colloquio modificato: verificare le varianti del doppiaggio")
        total = int(maximum[1])
        for count in range(total + 1):
            yield formats[0] % (count, total - count, careers[0 if count >= int(threshold[1]) else 1])


def read_catalog(path):
    """Cataloghi dei capitoli con dialoghi in tabelle, non nelle chiamate Say inline."""
    rows = json.loads(Path(path).read_text(encoding="utf-8"))
    if not isinstance(rows, list):
        raise ValueError(f"{path}: atteso un elenco di battute")
    for index, row in enumerate(rows):
        if not isinstance(row, dict) or row.get("speaker") not in VOICES:
            raise ValueError(f"{path}, battuta {index}: personaggio non valido")
        if not isinstance(row.get("text"), str) or not row["text"].strip():
            raise ValueError(f"{path}, battuta {index}: testo vuoto o non valido")
        if "key" in row and row["key"] != key(row["text"]):
            raise ValueError(f"{path}, battuta {index}: chiave diversa da quella usata dal gioco")
        yield row["speaker"], row["text"].strip(), row.get("kind", "dialogue")


def main():
    day2 = "--day2" in sys.argv or "--all" in sys.argv
    hints = "--no-hints" not in sys.argv
    lines, seen = [], set()

    def add(speaker, text, kind):
        text = text.strip()
        if speaker in VOICES and text and (speaker, text) not in seen:
            seen.add((speaker, text))
            lines.append({"speaker": speaker, "text": text, "key": key(text), "kind": kind})

    for rel in SOURCES:
        path = os.path.join(ROOT, rel)
        if not os.path.exists(path):
            continue
        src = open(path, encoding="utf-8").read()
        for m in re.finditer(r"\bSay\(", src):
            line_start = src.rfind("\n", 0, m.start()) + 1
            if src[line_start:m.start()].lstrip().startswith("void "):
                continue  # la definizione della funzione
            args, _ = split_args(src, m.end() - 1)
            if len(args) < 2 or not args[1].startswith("{"):
                continue
            # Riga logica che contiene la chiamata: serve a capire personaggio e giornata.
            head = src[line_start:m.start()]
            context = src[max(0, m.start() - 1500):m.start()]
            later = bool(re.search(r"(Phase|N)\s*==\s*1\d\d|SetPhase\(1\d\d\)|SubnetStep", head)) or \
                bool(re.search(r"if\((N|Phase)==1\d\d\)\s*\{?\s*$", context[-60:]))
            speakers = literals(args[0])
            if not speakers and args[0] == "Id":
                ids = re.findall(r'Id==TEXT\("(\w+)"\)', head)
                speakers = ids[-1:]
            items, _ = split_args(args[1], 0)
            for item in items:
                for text in literals(item):
                    for speaker in speakers:
                        if later and not day2:
                            continue
                        add(speaker, text, "arrival" if rel.endswith("ClaraArrivalInterview.cpp") else "day2" if later else "dialogue")
        if rel.endswith("ClaraArrivalInterview.cpp"):
            for text in interview_variants(src):
                add("Alessia", text, "arrival")
        if hints:
            m = re.search(r"FString Day1Hint\(int32 Phase,int32 Level\)\s*\{", src)
            if m:
                body, _ = split_args(src, m.end() - 1)
                for text in literals(" ".join(body).replace("+", " ")):
                    add("Luca", text, "hint")
    catalogs = list((Path(ROOT) / "Content/Data/Dialogue").glob("*.json")) if "--all" in sys.argv else []
    for index, arg in enumerate(sys.argv):
        if arg == "--catalog":
            if index + 1 == len(sys.argv):
                raise ValueError("--catalog richiede un file JSON")
            catalogs.append(Path(sys.argv[index + 1]))
    for path in sorted(set(catalogs)):
        for speaker, text, kind in read_catalog(path):
            add(speaker, text, kind)
    json.dump(lines, sys.stdout, ensure_ascii=False, indent=1)
    by = {}
    for item in lines:
        by.setdefault(item["speaker"], [0, 0])
        by[item["speaker"]][0] += 1
        by[item["speaker"]][1] += len(item["text"])
    print("\n".join(f"{name}: {n} battute, {chars} caratteri" for name, (n, chars) in sorted(by.items())), file=sys.stderr)


if __name__ == "__main__":
    main()
