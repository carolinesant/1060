/*
progresjonsTre holder oversikt over antall ganger man har vært på språkkafe ved å lyse opp tilsvarende like mange grener. 
For å logge dette tar man med seg et blad, scanner det på språkkafeen og kommer tilbake og scanner det på treet.

Da leses informasjon fra blokk 8 i rfid chippen for å se om bladet er aktivert (da vil dataen være '1'), dersom den er det
vil den samsvarende grenen til bladet lyse opp og bladet vil nullstilles i blokk8 (dataen '1' blir byttet med '0');

Når alle grenene lyser er en iterasjon ferdig, da blir det skrevet en '1' til blokk 9 i chippen som ble scannet sist. Denne informasjonen vil 
leses på språkkafeen og man vil dermed få en premie der. 
I tillegg vil metoden iterasjonFerdig() kalles og det vil komme et lyd/lys-"show" på treet for å signalisere at iterasjonen er ferdig. 

I tillegg vil en PIR bevegelsessensor loggføre antall ganger det har vært bevegelse forbi treet og kalle på paminnelse metoden 
når det er over 10 ganger, da vil det komme en annen lyd-lys fremvisning for å minne brukeren på å dra på språkkafeen. 
 */

//---------------------------------------------------------------------------------------------

//Importerer SPI og MFRC522 bibloteket
#include <SPI.h>
#include <MFRC522.h>

//Definerer de to pin-ene fra rfid/mfrc55-en som trengs for å lage en instans
#define RST_PIN 9
#define SDA_PIN 10
MFRC522 rfid(SDA_PIN, RST_PIN); //Lager en instans av rfid


//Definerer hva de forskjellige digitale portene leder til
//Grenene er 2 og 2 LED-lys i kretsen
int gren1 = 2;
int gren2 = 3;
int gren3 = 4;
int gren4 = 5;
int piezo = 6;
int bevegelsesSensor = 8;


//Her defineres bladene som skal aktivere de ulike grene med UID til chippene deres som verdi. 
String blad1 = "0904918003"; //ultralight chip
String blad23 = "1194103201"; //brikke
String blad4 = "1953617520"; //kort

/*
For denne prototypen har vi slått sammen blad2 og blad3 til et blad, blad23, som vil lyse opp gren2 og gren3, dette er for å forenkle prototypen 
litt til det formålet som den skal brukes til. Det er ikke nødvendig at alle bladene funker for å vise frem funksjonalitet.
I den tiltenkte løsningen er det meningen at blad 2 skal aktivere gren 2 og blad 3 aktivere gren3, og at disse ikke er slått sammen. 

I tillegg er det meningen at hvert blad skal ha en ultralight chip på baksiden, i forbindelse med denne prototypen har vi bare fått tak i
bare en av disse. Isteden bruker vi et rfid kort og brikke for de andre bladene. Dermed vil du senere i koden se at vi sjekker hva slags
type det som har blitt scannet er, siden de bruker ulike "Write"-metoder. Hvis vi bare hadde hatt Ultralight chipper ville koden bare vært
annderledes ved at den ikke hadde if-sjekker for å sjekke PICC type, og det ville bare blitt brukt MIFARE_Ultralight_Write metoden
*/

//Definerer antBevegelser som skal brukes til å holde oversikt over hvor mange nye bevegelser forbi PIR sensoren som oppstår
//Og definerer variablen som skal holde statusen til pirSensoren, som starter på lav, siden det ikke er bevegelse forbi når man starter den
int antBevegelse = 0;
int pirSensorStatus = LOW;

//variablene tilstand0 og tilstand1 skal brukes for å skrive over informasjon i blokkene til chippen som blir scannet
byte tilstand0[] = {0x30};
byte tilstand1[] = {0x31};

void setup() {

  //Definerer pinMode for alle grene (led-lysene) og bevegelsessensoren
  pinMode(gren1,OUTPUT);
  pinMode(gren2,OUTPUT);
  pinMode(gren3, OUTPUT);
  pinMode(gren4, OUTPUT);
  pinMode(bevegelsesSensor,INPUT);

//Begynner SPI bus og initaliserer rfid scanner instans
  SPI.begin();
  rfid.PCD_Init();
}

void loop() {

//Sjekker om det finnes ny bevegelse forbi treet og hvis det er det øker antall bevegelser
/* På PIR sensoren kan man justere en tids-delay, som gjør at statusen holder seg HIGH i en stund etter den oppdager bevegelse, 
på denne måten unngår man at den øker hver gang man beveger seg forbi den i et kort tidrom,
(som når man for eksempel skal sette inn bladet eller ta et blad), dermed trenger man ikke tenke på dette i koden */

  
  if (digitalRead(bevegelsesSensor) != pirSensorStatus) { //Sjekker om bevegelsesSensoren har ulik verdi fra forrige verdi
      pirSensorStatus = digitalRead(bevegelsesSensor); //Setter stauts verdien til den nye verdien
      if (pirSensorStatus == HIGH) antBevegelse++; //hvis statusen er høy legges det til en ny i bevegelsestelleren    
  }
  
//Sjekker om antall bevegelser er over 10, da kalles paaminnelse metoden for å minne bruker på at det er lenge siden de dro på språkkafe
  if (antBevegelse >= 10) paaminnelse();
  

//Sjekker om det finnes noe som kan scannes på RFID scanneren (og prøver dermed å lese den) ellers avsluttes loopen
  if ( ! rfid.PICC_IsNewCardPresent() || ! rfid.PICC_ReadCardSerial() ) return;

//Definerer variablene chipUID
  String chipUID = "";

//Leser inn UID-en til chippen og lagrer den i chipUID
  for (byte i = 0; i < rfid.uid.size; i++) chipUID += rfid.uid.uidByte[i];

//Skal bruke StatusCode for å sjekke om lesing/skriving er godkjent
  MFRC522::StatusCode status;

//Lager en byte-array som informasjonen som leses skal skrives til, og en lengde på antall byte som skal leses
//Med MIFARE_Read metoden må man lese 18 bytes om gangen, derfor er byte-arrayen og lengden 18
  byte aktiveringsVerdi[18];
  byte lengde = 18;

//Sjekker om PICC typen til den leste chippen er MIFARE_1K, da er det enten brikken eller kortet som har blitt scannet
//For chipper med MIFARE_1K må man autentisere før man prøver å lese informasjon eller skrive informasjon dermed blir dette gjort
  if (rfid.PICC_GetType(rfid.uid.sak) == MFRC522::PICC_TYPE_MIFARE_1K) {
       
       MFRC522::MIFARE_Key key;
       for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

      //Autentiserer nøkkelen for blokk 8 og hvis det feiler så avsluttes loopen 
      status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 8, &key, &(rfid.uid));
      if (status != MFRC522::STATUS_OK) {
           operasjonFeil();
           rfid.PICC_HaltA(); // Halt PICC
           rfid.PCD_StopCrypto1();  // Stop encryption on PCD
           return;
      }   
  } 
  
//Leser av informasjonen lagret i blokk 8 på chippen, som er bladets aktiverings informasjon.
  status = rfid.MIFARE_Read(8, aktiveringsVerdi, &lengde);
  if (status != MFRC522::STATUS_OK) { //Hvis den ikke klarer å lese av informasjonen stopper loopen
    operasjonFeil();
    rfid.PICC_HaltA(); // Halt PICC
    rfid.PCD_StopCrypto1();  // Stop encryption on PCD
    return;
  }
 
//Sjekker om chippen har riktig aktiveringsVerdi som er 1, hvis ikke stopper loopen
  if (!(char(aktiveringsVerdi[0]) == '1')){
      operasjonFeil();
      rfid.PICC_HaltA(); // Halt PICC
      rfid.PCD_StopCrypto1();  // Stop encryption on PCD
      return;
  }

//"Nullstiller" aktiveringsVerdien i blokk 8 på chippen, slik at den blir '0' igjen
//Her sjekkes det igjen om chippen er av typen MIFARE_1K, brikken eller kortet, siden den og Ultralight chippen bruker ulike Write metoder.
  
  if (rfid.PICC_GetType(rfid.uid.sak) == MFRC522::PICC_TYPE_MIFARE_1K) status = rfid.MIFARE_Write(8, tilstand0, 16); //For brikke/kort
  else status = rfid.MIFARE_Ultralight_Write(8, tilstand0, 4); //For ultralight chip
  
  if (status != MFRC522::STATUS_OK) { //Hvis skrivingen feiler, avsluttes loopen
      operasjonFeil();
      rfid.PICC_HaltA(); // Halt PICC
      rfid.PCD_StopCrypto1();  // Stop encryption on PCD
      return;
  }

//Kaller metoden lysOppTre med chipen sin UID, denne metoden vil lyse opp grenen som hører til bladet med denne UID-en
  lysOppTre(chipUID);


//Sjekkes om alle grenene lyser, hvis de gjør det betyr det at iterasjonen er ferdig
//Da skrives det '1' i blokk 9 på bladet som ble scannet, denne verdien signaliserer at treet har gått gjennom en iterasjon og skal få en premie
//Denne verdien leses på språkkafeen, slik at man kan få premien sin neste gang man drar dit.
//I tillegg kalles iterasjonFerdig metoden som spiller en melodi og resetter lysene

  if ((digitalRead(gren1) == HIGH) && (digitalRead(gren2) == HIGH) && (digitalRead(gren3) == HIGH) && (digitalRead(gren4) == HIGH)) {

       //Her sjekkes det igjen om det er MIFARE_1K eller ikke for å bruke riktig Write metode
       if (rfid.PICC_GetType(rfid.uid.sak) == MFRC522::PICC_TYPE_MIFARE_1K) status = rfid.MIFARE_Write(9, tilstand1, 16);
       else status = rfid.MIFARE_Ultralight_Write(9, tilstand1, 4);
        
       if (status != MFRC522::STATUS_OK) { //Dersom skrivingen ikke er velykket, da vil loopen avsluttes
           operasjonFeil();
           rfid.PICC_HaltA(); // Halt PICC
           rfid.PCD_StopCrypto1();  // Stop encryption on PCD
       }            
       iterasjonFerdig();
  }

  rfid.PICC_HaltA(); // Halt PICC
  rfid.PCD_StopCrypto1();  // Stop encryption on PCD
}


//Lyser opp grenen som bladet med den riktig uid-en tilhører. 
void lysOppTre(String uid) {

    if(uid == blad1) digitalWrite(gren1,HIGH);
    if(uid == blad23) {digitalWrite(gren2,HIGH); digitalWrite(gren3, HIGH); }
    if(uid == blad4) digitalWrite(gren4,HIGH);

    tone(piezo,300,300);
    antBevegelse = 0; //Når man scanner nytt blad så må det gå 10 nye bevegelser før påminnelse metoden blir kalt
}

//Det spilles av en melodi og lysene blinker for å signalisere at iterasjonen er ferdig
void iterasjonFerdig() {
  
  delay(300); //Venter til det siste lyset får lyst litt for seg selv
  
  //Definerer en int arrayer som inkluderer tonene som skal spilles og lengden på notene og rekkefølgene lysene skal skus av og på
  int melodi[] = {262, 196,196, 220, 196,0, 247, 262};
  int toneLengder[] = {250, 125, 125, 250, 250, 250, 250, 250};
  int lysRekke[] = {gren1, gren2, gren3, gren4, gren1, gren2, gren3, gren4};

  //Spiller melodien og lyser opp lysene fire ganger etter hverandre med en loop
   for (int gang = 0; gang < 4; gang++) {
      
      //For-loopen itererer gjennom arrayene for å spille av hver tone og skrur av og på hvert lys i riktig rekkefølge
      for (int i = 0; i < 8; i++) {
        tone(piezo, melodi[i],toneLengder[i]);
        digitalWrite(lysRekke[i], HIGH);
        int pauseMellomToner = toneLengder[i] * 1.30;
        delay(pauseMellomToner);
        noTone(piezo);
        digitalWrite(lysRekke[i], LOW);
      }
   }
    reset(); //Så kalles metoden reset
}


//reset slukker alle lysene og nulstiller antBevegelser
void reset() {
    digitalWrite(gren1,LOW);
    digitalWrite(gren2,LOW);
    digitalWrite(gren3,LOW);
    digitalWrite(gren4,LOW);
    antBevegelse = 0;
}


//paaminnelse spiller en melodi og blinker med lysene for å signalisere at det er på tide å dra på språkkafe igjen
void paaminnelse() {

    //Finner statusen til alle lysene/grenene ved å bruke digitalRead, for å kunne stille lysene tilbake riktig etter lys-blinking
    int gren1Status = digitalRead(gren1);
    int gren2Status = digitalRead(gren2);
    int gren3Status = digitalRead(gren3);
    int gren4Status = digitalRead(gren4);
    
    //Definerer en int arrayer, en som inkluderer tonene som skal spilles og en som har rekkefølgen lysene skal skrus av 
    int melodi[] = {240, 230, 240, 230};
    int lysRekke[] = {gren4, gren3, gren2, gren1};
    
    //Skrur alle lysene på og venter litt
    digitalWrite(gren1, HIGH); digitalWrite(gren2, HIGH); digitalWrite(gren3, HIGH); digitalWrite(gren4, HIGH);
    delay(200);
     
    //Spiller melodien og lyser opp 3 ganger ved hjelp av en for-loop
     for (int gang = 0; gang < 3; gang++) {
        
        //For-loopen itererer gjennom arrayene for å spille av hver tone og skrur av lyset neste i rekken
        for (int i = 0; i < 4; i++) {
          tone(piezo, melodi[i],290);
          digitalWrite(lysRekke[i], LOW);
          int pauseMellomToner = 290* 1.30;
          delay(pauseMellomToner);
          noTone(piezo);
        }
        //Skrur alle lysene på igjen og spiller en tone og venter
        digitalWrite(gren1, HIGH); digitalWrite(gren2, HIGH); digitalWrite(gren3, HIGH); digitalWrite(gren4, HIGH);
        tone(piezo, 200, 300);
        delay(350);
     }

     //Setter alle lysene tilbake til sin orginale status før paminnelse kallet
      digitalWrite(gren1, gren1Status);
      digitalWrite(gren2, gren2Status);
      digitalWrite(gren3, gren3Status);
      digitalWrite(gren4, gren4Status);
    
      //Nulstiller antall bevegelser
      antBevegelse = 0;
    
}


//Spiller en tone som signaliserer at noe har skjedd feil underveis i tilkoblingen, lesningen eller skrivingen til rfid chippen
void operasjonFeil() {
  tone(piezo, 100, 150);
  delay(200);
  tone(piezo, 50, 150);
}
