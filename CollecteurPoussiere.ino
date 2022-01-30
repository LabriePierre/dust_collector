// Code de contrôle d'aspirateur couplé à des outils tel un planeur et dégauchisseuse.
// Cumule les nombre d'heure de fonctionnement de chaque appareil.

// Pierre Labrie, 2022/01/28, Licence GNU GPL V3
// EmonLibrary examples openenergymonitor.org, Licence GNU GPL V3

#include "EmonLib.h"      // librairie pour  Emon
#include <TM1637.h>       // librairie pour TM1637 affichage LED 7 segments
#include <EEPROM.h>       // librairie pour enregistrement dans EEPROM


// Pin d'entree pour senseurs de courant
#define PIN_LECTURE_0   A0   // Pin analogue
#define PIN_LECTURE_1   A2   // Pin analogue
#define PIN_LECTURE_2   A4   // Pin analogue

// relais de démarrage collecteur de poussière
#define DC_RELAIS 8       // Pin relais

// pour sélecteur binaire d'affichage
#define SELECT_0  10       // pin 1
#define SELECT_1  11       // pin 2
#define SELECT_2  12       // pin 3

// voyant lumineux
#define AFFICHE_0  5        // OUTIL 1
#define AFFICHE_1  6        // OUTIL 2
#define AFFICHE_2  7        // OUTIL 3

// pour module LED 7 segments
#define  LED_PIN_CLK  2    // CLK pin  pour LED GND
#define  LED_PIN_DIO  3    // DIO pin  pour LED GND

// module LED 7 segments
TM1637   tm(LED_PIN_CLK, LED_PIN_DIO);

// Nombre d'outils monitorés
const int NOMBRE_OUTILS = 3;

// variables globales pour mesure du temps d'utilisation. Initialiser selon nombre d'outil
unsigned long tempsEcoule[NOMBRE_OUTILS] = {0, 0, 0}; // en millisecondes

//calcul
int secondes[NOMBRE_OUTILS] = {0, 0, 0};
int minutes[NOMBRE_OUTILS] = {0, 0, 0};

//cumule
long minutesTotales[NOMBRE_OUTILS] = {0, 0, 0};
int minutesNouv[NOMBRE_OUTILS] = {0, 0, 0};
int secondesNouv[NOMBRE_OUTILS] = {0, 0, 0};

//Statut des boutons et outils
boolean powerDetected[NOMBRE_OUTILS] = {0, 0, 0};
boolean collectorIsOn = 0;

//Temporisation du collecteur de poussiere
int DC_spindown = 50;
int DC_spinUP = 10;

// niveau de démarrage de l'outil
double ampThreshold = 0.80;

// Creer les instances du moniteur de courant EnergyMonitor
EnergyMonitor emon0;
EnergyMonitor emon1;
EnergyMonitor emon2;

void setup() {
  //initialisation des pins
  pinMode(DC_RELAIS, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  pinMode(AFFICHE_0, OUTPUT);
  pinMode(AFFICHE_1, OUTPUT);
  pinMode(AFFICHE_2, OUTPUT);

  digitalWrite(DC_RELAIS, LOW);
  digitalWrite(LED_BUILTIN, LOW);



  //pins du sélecteur binaire
  pinMode(SELECT_0, INPUT_PULLUP);
  pinMode(SELECT_1, INPUT_PULLUP);
  pinMode(SELECT_2, INPUT_PULLUP);

  tm.init();              // initialise l'affichage LED 7 segments
  tm.set(2);              // ajuste la luminosité (1 à 7)
  displayNumber(8888);    // affiche 8888 pour validation de tous les segments


  Serial.begin(9600);
  while (!Serial) ; // attente de l'ouverture du port série Arduino

  //  remise à zéro du compteur pour montage initial
  //  writeLongIntoEEPROM(100, minutesTotales[0]);
  //  writeLongIntoEEPROM(200, minutesTotales[0]);
  //  writeLongIntoEEPROM(300, minutesTotales[0]);

  // lecture du dernier état avant une panne électrique
  // réinistalise les variables globales
  minutesTotales[0] = readLongFromEEPROM(100);
  minutesTotales[1] = readLongFromEEPROM(200);
  minutesTotales[2] = readLongFromEEPROM(300);


  // configuration Emon pour module Hall ACS712ELC-20A 8.3
  // configuration Emon pour module Hall SCT 013-30 111.1
  emon0.current(PIN_LECTURE_0, 60);             // Current: input pin, calibration.
  emon1.current(PIN_LECTURE_1, 60);
  emon2.current(PIN_LECTURE_2, 60);
}

void loop() {


  //verifie le fonctionnement de tous les outils
  bool activeTool[NOMBRE_OUTILS] = {0, 0, 0};

  for (int i = 0; i < NOMBRE_OUTILS; i++) {
    if (checkForAmperageChange(i)) {
      activeTool[i] = true;

      if (powerDetected[i]  == false) {
        tempsEcoule[i] = millis();
      }
      powerDetected[i]  = true;
      lumieresTemoins(true,i);
    }
  }

  //démarre l'aspirateur selon l'état de n'importe quel outil.
  if (activeTool[0] || activeTool[1] || activeTool[2] ) {

    if (collectorIsOn == false) {
      delay(DC_spinUP);
      turnOnDustCollection();
    }
  }
  else
  {
    if (collectorIsOn == true) {
      delay(DC_spindown);
      turnOffDustCollection();
    }
  }

  //calcul du temps écoulé par outil et cumule les minutes totales dans le registre
  for (int i = 0; i < NOMBRE_OUTILS; i++) {

    if (powerDetected[i] == true && activeTool[i] == false ) {

      calculTempsOutil(i);
      powerDetected[i] = false;
      lumieresTemoins(false,i);
      cumulTempsOutil(i);
    }
  }

  //debug();

  // affiche les heures sur led selon le sélecteur
  afficheSelection(analyseSelecteurBinaire());

}

//vériifie les position du sélecteur binaire
//Retourne le numéro correspondant de l'affichage du sélecteur
int analyseSelecteurBinaire()
{
  if (!verif_Interrup(SELECT_0) && verif_Interrup(SELECT_1) && verif_Interrup(SELECT_2)) {
    return 6;
  }
  if (verif_Interrup(SELECT_0) && !verif_Interrup(SELECT_1) && verif_Interrup(SELECT_2)) {
    return 5;
  }
  if (!verif_Interrup(SELECT_0) && !verif_Interrup(SELECT_1) && verif_Interrup(SELECT_2)) {
    return 4;
  }
  if (verif_Interrup(SELECT_0) && verif_Interrup(SELECT_1) && !verif_Interrup(SELECT_2)) {
    return 3;
  }
  if (!verif_Interrup(SELECT_0) && verif_Interrup(SELECT_1) && !verif_Interrup(SELECT_2)) {
    return 2;
  }
  if (verif_Interrup(SELECT_0) && !verif_Interrup(SELECT_1) && !verif_Interrup(SELECT_2)) {
    return 1;
  }
  if (!verif_Interrup(SELECT_0)  && !verif_Interrup(SELECT_1)  && !verif_Interrup(SELECT_2) ) {
    return 0;
  }
}


// Affiche sur le LED la valeur en minutes ou heures pour l'outil sélectionné
void afficheSelection(int selection)
{
  switch (selection) {

    case 0:
      displayNumber(8888);
      break;
    case 1:
      displayNumber(minutesTotales[0] / 60);
      break;
    case 2:
      displayNumber(minutesTotales[1] / 60);
      break;
    case 3:
      displayNumber(minutesTotales[2] / 60);
      break;
    case 4:
      displayNumber(minutesTotales[0]);
      break;
    case 5:
      displayNumber(minutesTotales[1]);
      break;
    case 6:
      displayNumber(minutesTotales[2]);
      break;

    default:
      displayNumber(8888);
  }

}

//affichage du nombre de minutes ou heures dans le led 7 segments
// supporte un nombre de plus de 4 chiffres en omettant la partie de gauche
void displayNumber(long numNouv) {

  tm.display(3, numNouv % 10);
  tm.display(2, numNouv / 10 % 10);
  tm.display(1, numNouv / 100 % 10);
  tm.display(0, numNouv / 1000 % 10);
}


// Vérification du passage de courant dans le module Hall
// Vérifie un des trois outils prévus
boolean checkForAmperageChange(int outil) {

  double Irms = 0;
  // Calculer Irms seulement
  switch (outil) {

    case 0:
      Irms = emon0.calcIrms(2960);
      break;

    case 1:
      Irms = emon1.calcIrms(2960);
      break;

    case 2:
      Irms = emon2.calcIrms(2960);
      break;

    default:
      Irms = 0;
  }

  // pour debug
  //      Serial.print("outil: ");
  //      Serial.println(outil);
  //      Serial.print(Irms);
  //      Serial.println(" Amps RMS");


  // si le courant dépasse le niveau minimum, retourne vrai
  if (Irms > ampThreshold) {
    return true;
  } else {
    return false;
  }
}

//Ouvre aspirateur
void turnOnDustCollection() {
  Serial.println("turnOnDustCollection");
  digitalWrite(DC_RELAIS, HIGH);
  digitalWrite(LED_BUILTIN, HIGH);
  collectorIsOn = true;
}

//Ferme aspirateur
void turnOffDustCollection() {
  Serial.println("turnOffDustCollection");
  digitalWrite(DC_RELAIS, LOW);
  digitalWrite(LED_BUILTIN, LOW);
  collectorIsOn = false;
}


//Calculer le temps d'opération écoulé
//Utilise la variable globale tempsEcoule initialisée à la détection de l'outil
void calculTempsOutil(int outil) {

  if (tempsEcoule[outil] > 0 )
  {
    secondes[outil] = (millis() - tempsEcoule[outil]) / 1000;
    minutes[outil] = secondes[outil] / 60;
    secondes[outil] %= 60;
  }
}



//Totalise le temps global d'opération
//Mémorise dans EEPROM le nombre global de minutes pour garder les données du compteur en cas de panne de courant.
void cumulTempsOutil(int outil)
{
  //test arbitraire pour s'assurer qu'on est dans une mesure distincte
  if (!((minutesNouv[outil] + secondesNouv[outil]) ==  (minutes[outil] + secondes[outil])))
  {

    //cumule les heures pour un outil
    minutesTotales[outil] = minutesTotales[outil] + minutes[outil];

    //mise en mémoire des repères pour prochaine itération
    minutesNouv[outil] =  minutes[outil] ;
    secondesNouv[outil] = secondes[outil];


    // enregiste à toutes les fermetures d'un outil dans la mémoire correspondante
    if ( minutesTotales[outil] > 0 )
    {

      switch (outil) {
        case 0:
          writeLongIntoEEPROM(100, minutesTotales[outil]);;
          break;

        case 1:
          writeLongIntoEEPROM(200, minutesTotales[outil]);;
          break;

        case 2:
          writeLongIntoEEPROM(300, minutesTotales[outil]);;
          break;
      }

    }

  }
}

//Écriture en mémoire du total d'heures cumulées
void writeLongIntoEEPROM(int address, long number)
{
  EEPROM.write(address, (number >> 24) & 0xFF);
  EEPROM.write(address + 1, (number >> 16) & 0xFF);
  EEPROM.write(address + 2, (number >> 8) & 0xFF);
  EEPROM.write(address + 3, number & 0xFF);
}



//Lecture de la mémoire du total d'heures cumulées
long readLongFromEEPROM(int address)
{
  return ((long)EEPROM.read(address) << 24) +
         ((long)EEPROM.read(address + 1) << 16) +
         ((long)EEPROM.read(address + 2) << 8) +
         (long)EEPROM.read(address + 3);
}


// validation du statut d'un interrupteur
bool verif_Interrup(int pin)
{
  bool retour =  false;

  if (digitalRead(pin) == LOW ) {
    retour =  true;
  } else {
    retour = false;
  }

  return retour;
}

// controle les lumières témoins pour indiquer le fonctionnement de l'outil
void lumieresTemoins(bool bouton, int outil)
{

  switch (outil) {

    case 0:
      if (bouton) {
        digitalWrite(AFFICHE_0, HIGH);
      }
      else
      { digitalWrite(AFFICHE_0, LOW);
      }
      break;
      
    case 1:
      if (bouton) {
        digitalWrite(AFFICHE_1, HIGH);
      }
      else
      { //digitalWrite(AFFICHE_1, LOW);
      }
      break;

    case 2:
      if (bouton) {
        digitalWrite(AFFICHE_2, HIGH);
      }
      else
      { digitalWrite(AFFICHE_2, LOW);
      }
      break;

  }
}


void debug() {
  //effectuer l'affichage selon le sélecteur

  for (int i = 0; i < NOMBRE_OUTILS; i++) {
    Serial.print("heure/minutes ");
    Serial.print(i);
    Serial.print(" : ");
    Serial.print(minutesTotales[i] / 60);
    Serial.print("/");
    Serial.println(minutesTotales[i]);

  }
}
