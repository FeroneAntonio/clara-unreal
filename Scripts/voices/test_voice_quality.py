import unittest

from voice_quality import accepted, italian_number, quality, spoken, retry_seed_offset


class VoiceQualityTests(unittest.TestCase):
    def test_ipv4_is_pronounced_with_italian_numbers(self):
        self.assertEqual(spoken("ping 192.168.1.11"),
                         "ping centonovantadue punto, centosessantotto punto, uno punto, undici")
        self.assertEqual(italian_number(255), "duecentocinquantacinque")
        self.assertEqual(italian_number(180), "centottanta")

    def test_asr_acronyms_and_numbers_do_not_trigger_false_retries(self):
        record = {"text": "PC2 usa TCP/IP, ICMP e 48 bit.",
                  "heard": "PC 2 usa TCPIP, ICMP e quarantotto bit."}
        self.assertTrue(accepted(record))
        self.assertTrue(accepted({"text": "È della macchinetta del caffè.",
                                  "heard": "e della macchinetta del caffe."}))

    def test_wrong_address_is_rejected_even_in_a_long_correct_sentence(self):
        text = "Adesso apri il terminale del portatile sul tuo tavolo e scrivi ping 192.168.1.11 per verificare il collegamento."
        heard = text.replace("192.168.1.11", "1902.688.1.11")
        self.assertFalse(quality(text, heard)[1])
        self.assertFalse(accepted({"text": text, "heard": heard}))
        self.assertTrue(accepted({"text": text, "heard": text}))

    def test_missing_sentence_is_rejected(self):
        self.assertFalse(accepted({"text": "Prendi il cavo e collegalo al portatile.", "heard": "Prendi il cavo."}))
        self.assertFalse(accepted({"text": "Prendi il cavo."}))

    def test_new_routing_devices_keep_the_same_spoken_names(self):
        self.assertEqual(spoken("Collega R2 a SW2."), "Collega erre due a switch due.")
        self.assertTrue(accepted({"text": "SW2 non risponde a R2.",
                                  "heard": "Switch due non risponde a erre due."}))

    def test_company_name_is_spelled_and_cannot_be_omitted(self):
        self.assertEqual(spoken("Benvenuto in TNES."), "Benvenuto in ti, enne, e, esse.")
        text = "Benvenuto in TNES: reti costruite e protette, questo facciamo. Niente domande a raffica oggi, ti mettiamo subito al lavoro con la squadra."
        self.assertTrue(accepted({"text": text, "heard": spoken(text)}))
        self.assertFalse(accepted({"text": text, "heard": text.replace("TNES", "CLARA")}))

    def test_rejected_clip_resume_uses_new_seeds(self):
        self.assertEqual(retry_seed_offset(None, 4), 0)
        self.assertEqual(retry_seed_offset({"text": "Eccoti qui.", "heard": "Eccoti qui."}, 4), 0)
        rejected = {"text": "Eccoti qui.", "heard": "Eccomi.", "seed_offset": 4, "attempts": 4}
        self.assertEqual(retry_seed_offset(rejected, 4), 8)
        self.assertEqual(retry_seed_offset({"text": "Eccoti qui.", "heard": "Eccomi."}, 4), 4)

    def test_asr_word_boundaries_are_not_substituted_words(self):
        self.assertTrue(accepted({"text": "Bel lavoro oggi: MAC letto, ping fatto.",
                                  "heard": "Bellavoro oggi: macletto, ping fatto."}))
        self.assertTrue(accepted({"text": "Il portatile è collegato allo switch. E PC2?",
                                  "heard": "Il portatile è collegato allo switch. Epici 2?"}))
        self.assertFalse(accepted({"text": "Eccoti qui. Puntualissimo!",
                                   "heard": "Eccomi qui. Puntualissimo!"}))

    def test_network_notation_has_unambiguous_spoken_form(self):
        self.assertEqual(spoken("IPv4, Gi0/1, TTL, Cisco123"),
                         "ai pi versione quattro, gigabit zero barra uno, ti ti elle, Cisco centoventitre")
        self.assertEqual(spoken("10.x.x.x e 172.16"),
                         "dieci punto, ics punto, ics punto, ics e centosettantadue punto, sedici")
        self.assertEqual(spoken("FF-FF-FF-FF-FF-FF"), "effe effe, ripetuto sei volte")
        self.assertEqual(spoken("ip neigh flush all; arp -d *; arp -a"),
                         "ai pi nei flash ol; arp trattino di asterisco; arp trattino a")
        self.assertEqual(spoken("> utente, # privilegiato, (config)#, ? e Tab"),
                         "maggiore utente, cancelletto privilegiato, config tra parentesi, cancelletto, punto interrogativo e Tab")

    def test_asr_spacing_preserves_credentials_and_ip(self):
        self.assertTrue(accepted({"text": "Premi T e scrivi ping 192.168.1.11",
                                  "heard": "Premiti e scrivi ping192.168.1.11"}))
        self.assertTrue(accepted({"text": "Imposta Cisco123 e Console123.",
                                  "heard": "Imposta Cisco 123 e Console 123."}))
        text = "Imposta nome R2, enable secret Cisco123, password Console123 e login sulla console. Sono credenziali di questo laboratorio."
        self.assertFalse(accepted({"text": text, "heard": text.replace("Cisco123", "Cisco124")}))
        text = "Gli indirizzi privati sono 10.x.x.x, da 172.16 a 172.31 e 192.168.x.x. Si riusano nelle reti locali."
        self.assertFalse(accepted({"text": text, "heard": text.replace("172.31", "172.30")}))


if __name__ == "__main__":
    unittest.main()
