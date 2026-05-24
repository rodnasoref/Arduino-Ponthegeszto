# Profi Akkumulátor Ponthegesztő Vezérlés (Spot Welder v1.4.1)

##### 

##### Ez a projekt egy professzionális, Arduino-alapú, kétimpulzusos (Dual-Pulse) akkumulátor ponthegesztő vezérlő szoftvere. A rendszer kifejezetten biztonságos és stabil működésre lett optimalizálva, valós idejű OLED megjelenítéssel és megszakítás-alapú (interrupt) enkóderkezeléssel. 

##### 

##### Fejlesztő: Rodnas Oref 

##### 

##### 🌟 Főbb Jellemzők

##### 

##### &#x20; 

##### &#x20; Kétimpulzusos (Dual-Pulse) hegesztés:   Paraméterezhető előhegesztés, szünetidő és főhegesztés a tökéletes kötésekért. 

##### 

##### 

##### &#x20;

##### Kettős üzemmód: Automatikus (érintésvezérelt) és Manuális indítási lehetőség. 

##### 

##### 

##### &#x20;

##### Akkumulátor védelem: Folyamatos feszültségmérés; letiltja a hegesztést, ha az akkumulátor feszültsége kritikus szint (11.0V) alá esik, vagy ha egyáltalán nincs csatlakoztatva (<1.5V). 

##### 

##### 

##### &#x20;

##### Automatikus mentés: A beállított paramétereket 3 másodperccel a módosítás után automatikusan elmenti az EEPROM-ba. 

##### 

##### 

##### &#x20;

##### Vizuális visszajelzés: OLED grafikus felület aktuális feszültségkijelzéssel, paraméterekkel és "Cooldown" várakozási sávval. 

##### 

##### 

##### 

##### \## 🔄 Újdonságok a v1.4.1-es verzióban

##### 

##### A stabilitás és a megbízhatóság érdekében az alábbi javítások kerültek beépítésre:

##### 

##### &#x20;I2C kommunikáció túlterhelés elleni védelme (FPS limiter beépítése, max \~25 FPS képfrissítés). 

##### 

##### 

##### &#x20;OLED boot késleltetés (250 ms) a "fekete képernyő" probléma elkerülésére bekapcsoláskor. 

##### 

##### 

##### &#x20;A `volatile int` változók `volatile byte` típusra lettek cserélve a megszakítások atomi olvasásának garantálása érdekében. 

##### 

##### 

##### &#x20;A képernyőfrissítés (Draw flag) törlési logikájának javítása az interrupt race-condition elkerülésére. 

##### 

##### 

##### 

##### \## 🔌 Hardver Kiosztás (Pinout)

##### 

##### A mikrokontroller lábainak bekötése a következőképpen alakul: 

##### 

##### | Funkció | Arduino Láb | Típus | Leírás |

##### | --- | --- | --- | --- |

##### | WELD\_OUT\_PIN | `D3` | Kimenet | Hegesztőjelet vezérlő MOSFET/Relé kimenet. 

##### 

##### &#x20;|

##### | VOLTAGE\_PIN | `A1` | Bemenet | Feszültségosztóról érkező jel az akku méréséhez. 

##### 

##### &#x20;|

##### | SENSE\_PIN | `A0` | Bemenet | A hegesztőkarok érintkezését vizsgáló szenzor. 

##### 

##### &#x20;|

##### | ENCODER\_SW | `D4` | Bemenet | Forgójeladó nyomógombja (belső felhúzó ellenállással). 

##### 

##### &#x20;|

##### | ENCODER\_CLK | `D5` | Bemenet | Forgójeladó CLK lába (belső felhúzó ellenállással). 

##### 

##### &#x20;|

##### | ENCODER\_DT | `D6` | Bemenet | Forgójeladó DT lába (belső felhúzó ellenállással). 

##### 

##### &#x20;|

##### | BTN\_BACK | `D8` | Bemenet | Vissza / Módválasztó gomb (belső felhúzó ellenállással). 

##### 

##### &#x20;|

##### | BTN\_CONFIRM | `D9` | Bemenet | Megerősítés / Manuális indítás gomb (belső felhúzó ellenállással). 

##### 

##### &#x20;|

##### 

##### \## ⚙️ Paraméterek és Beállítások

##### 

##### A kijelzőn három fő paraméter finomhangolható (mindegyik 0-99 ms közötti érték lehet): 

##### 

##### 1\. P1 (Pulse 1): Előhegesztés ideje. Célja a nikkel szalag rásütése az akkucellára, ami letisztítja az oxidréteget. Alapértelmezett: 3 ms. 

##### 

##### 

##### 2\. DLY (Delay): A két hegesztési impulzus közötti szünetidő. Alapértelmezett: 15 ms. 

##### 

##### 

##### 3\. P2 (Pulse 2): A főhegesztés ideje. Ez adja le a kötéshez szükséges fő áramot. Alapértelmezett: 15 ms. 

##### 

##### 

##### 

##### \## 🎮 Kezelés és Működés

##### 

##### &#x20;

##### Menü navigáció: A forgójeladó (enkóder) gombjának megnyomásával válthat a paraméterek (P1, DLY, P2) között. 

##### 

##### 

##### &#x20;

##### Értékek módosítása: Az enkóder tekerésével növelheti vagy csökkentheti a kiválasztott paraméter értékét. 

##### 

##### 

##### &#x20;

##### Módváltás: A BTN\_BACK (D8) gombbal válthat az `AUTO` és `MANUAL` módok között. 

##### 

##### 

##### &#x20;

##### Automatikus hegesztés (AUTO): Amikor az elektródák megfelelően érintkeznek a felülettel, a rendszer elindít egy 800 ms-os visszaszámlálást, majd végrehajtja a hegesztést. Ezt követően egy 1500 ms-os biztonsági (Cooldown) hűlési fázis lép életbe, ami meggátolja az akaratlan duplázást. Ha az érintkezés instabil 3000 ms-ig, a rendszer hibaállapotot ("ERROR: BAD CONTACT!") jelez. 

##### 

##### 

##### &#x20;

##### Manuális hegesztés (MANUAL): Ebben a módban a BTN\_CONFIRM (D9) gomb megnyomásával lehet kézzel elindítani a hegesztési ciklust. 

##### 

##### 

##### 

##### \## 📚 Szükséges Könyvtárak

##### 

##### A projekt lefordításához a következő Arduino könyvtárakra van szükség: 

##### 

##### &#x20;

##### `U8g2lib` - Az OLED kijelző (SH1106 128x64 I2C) vezérléséhez. 

##### 

##### 

##### &#x20;

##### `Wire` - Az I2C kommunikációhoz. 

##### 

##### 

##### &#x20;

##### `EEPROM` - A beállítások memóriába mentéséhez.

