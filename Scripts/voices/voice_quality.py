"""Pronuncia e confronto delle battute; nessun modello o dipendenza esterna."""
import difflib
import re
import unicodedata


def italian_number(number):
    """Interi degli indirizzi IPv4, pronunciati senza ambiguità dal TTS."""
    small = ("zero", "uno", "due", "tre", "quattro", "cinque", "sei", "sette", "otto", "nove",
             "dieci", "undici", "dodici", "tredici", "quattordici", "quindici", "sedici", "diciassette", "diciotto", "diciannove")
    tens = ("", "", "venti", "trenta", "quaranta", "cinquanta", "sessanta", "settanta", "ottanta", "novanta")
    if not 0 <= number <= 999:
        return str(number)
    if number < 20:
        return small[number]
    if number < 100:
        stem = tens[number // 10]
        unit = number % 10
        return (stem[:-1] if unit in (1, 8) else stem) + (small[unit] if unit else "")
    hundreds, rest = divmod(number, 100)
    stem = (small[hundreds] if hundreds > 1 else "") + "cento"
    if rest == 8 or 80 <= rest <= 89:
        stem = stem[:-1]
    return stem + (italian_number(rest) if rest else "")


SPOKEN = [
    (r"\b(?:\d+\.){3}\d+\b", lambda m: " punto, ".join(italian_number(int(n)) for n in m[0].split("."))),
    (r"\b(?:\d{1,3}|x)(?:\.(?:\d{1,3}|x)){1,3}\b",
     lambda m: " punto, ".join("ics" if n.lower() == "x" else italian_number(int(n)) for n in m[0].split("."))),
    (r"AA-BB-CC-DD-EE-FF", "A A, B B, C C, D D, E E, F F"),
    (r"FF-FF-FF-FF-FF-FF", "effe effe, ripetuto sei volte"),
    (r"\bIPv\s*4\b", "ai pi versione quattro"),
    (r"\bGi\s*(\d+)/(\d+)\b", lambda m: "gigabit " + italian_number(int(m[1])) + " barra " + italian_number(int(m[2]))),
    (r"\b(Cisco|Console)\s*123\b", r"\1 centoventitre"),
    (r"\bTTL\b", "ti ti elle"),
    (r"TCP/IP", "ti ci pi ai pi"), (r"\bICMP\b", "i ci emme pi"), (r"\bOSI\b", "osi"), (r"\bMAC\b", "mac"), (r"\bIP\b", "ai pi"),
    (r"\bLED\b", "led"), (r"\bUSB\b", "u esse bi"), (r"\bRJ-45\b", "erre gei quarantacinque"), (r"\bSFP\b", "esse effe pi"),
    (r"\bLC\b", "elle ci"), (r"\bSSH\b", "esse esse acca"), (r"\bARP\b", "arp"), (r"\bCLARA\b", "Clara"), (r"\bTNES\b", "ti, enne, e, esse"),
    (r"\bSW1\b", "switch uno"), (r"\bSW2\b", "switch due"), (r"\bSW-PROVA\b", "switch di prova"), (r"\bSW-A\b", "switch A"), (r"\bSW-B\b", "switch B"),
    (r"\bPC2\b", "pi ci due"), (r"\bPC3\b", "pi ci tre"), (r"\bR1\b", "erre uno"), (r"\bR2\b", "erre due"),
    (r"\b[Cc][Aa][Tt] 6\b", "cat sei"), (r"\bCONSOLE\b", "console"), (r"\bETHERNET\b", "ethernet"), (r"\bFIBRA\b", "fibra"), (r"\bTEST\b", "test"),
    (r"ipconfig /all", "ipconfig slash all"), (r"address-table", "address table"), (r"/(\d+)\b", r" barra \1"),
    (r"\bai pi neigh\b", "ai pi nei"), (r"\bflush all\b", "flash ol"),
    (r"\barp -d \*", "arp trattino di asterisco"), (r"\barp -a\b", "arp trattino a"),
    (r"\(config\)#", "config tra parentesi, cancelletto"), (r">", "maggiore"), (r"#", "cancelletto"),
    (r"(?<!\S)\?(?=\s+e\s+Tab\b)", "punto interrogativo"),
    (r"\b(con|premi|Premi) T\b", r"\1 ti"), (r"\b(premi|Premi) Q\b", r"\1 cu"), (r"\(I\)", ""), (r"\(N\)", "enne"),
    (r"[«»]", ""), (r"→", " verso "), (r"·", ","), (r"\s+", " "),
]


def spoken(text):
    text = re.sub(r"\bping(?=\d)", "ping ", text, flags=re.I)
    text = re.sub(r"\bi\s+cmp\b", "ICMP", text, flags=re.I)
    text = re.sub(r"\bTCP[ /]?IP\b", "TCP/IP", text, flags=re.I)
    text = re.sub(r"\bPC\s*(\d)\b", r"PC\1", text, flags=re.I)
    text = re.sub(r"\bCAT\s*(\d)\b", r"CAT \1", text, flags=re.I)
    text = re.sub(r"\bRJ-?45\b", "RJ-45", text, flags=re.I)
    for pattern, repl in SPOKEN:
        text = re.sub(pattern, repl, text, flags=re.I)
    return text.strip()


def words(text):
    text = spoken(text)
    text = re.sub(r"(?<=[a-zA-Z])(?=\d)|(?<=\d)(?=[a-zA-Z])", " ", text)
    text = re.sub(r"\b\d+\b", lambda m: italian_number(int(m[0])), text)
    text = "".join(c for c in unicodedata.normalize("NFD", text.lower()) if not unicodedata.combining(c))
    return re.sub(r"[^a-z0-9 ]", " ", text).split()


def quality(text, heard):
    expected_words, heard_tokens = words(text), words(heard)
    expected_compact, heard_compact = "".join(expected_words), "".join(heard_tokens)
    # Whisper may merge "bel lavoro" into "bellavoro"; identical characters are not missing speech.
    score = 1.0 if expected_compact == heard_compact else difflib.SequenceMatcher(None, expected_words, heard_tokens).ratio()
    required = re.findall(r"\b(?:\d{1,3}|x)(?:\.(?:\d{1,3}|x)){1,3}\b|\b(?:TNES|Cisco123|Console123)\b|FF-FF-FF-FF-FF-FF", text, flags=re.I)
    # Names, address ranges and training credentials must remain exact even in a long sentence.
    return score, all("".join(words(value)) in heard_compact for value in required)


def accepted(record, minimum=0.9):
    score, addresses_match = quality(record["text"], record.get("heard", ""))
    return score >= minimum and addresses_match


def retry_seed_offset(record, tries, minimum=0.9):
    """A resumed rejected line must explore new samples, rather than repeat the same failed seeds."""
    if not record or accepted(record, minimum):
        return 0
    return int(record.get("seed_offset", 0)) + int(record.get("attempts", tries))
