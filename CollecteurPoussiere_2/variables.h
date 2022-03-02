// variables globales et librairies

#include "EmonLib.h"      // librairie pour  Emon
#include <TM1637.h>       // librairie pour TM1637 affichage LED 7 segments
#include <EEPROM.h>       // librairie pour enregistrement dans EEPROM



// Pin d'entree pour senseurs de courant
#define PIN_LECTURE_0   A0   // Pin analogue
#define PIN_LECTURE_1   A2   // Pin analogue
#define PIN_LECTURE_2   A4   // Pin analogue

// relais de démarrage collecteur de poussière
#define DC_RELAIS_NO 8       // Pin relais NO
#define DC_RELAIS_NC 9       // Pin relais NC

// pour sélecteur binaire d'affichage
#define SELECT_0  10       // pin 1
#define SELECT_1  11       // pin 2
#define SELECT_2  12       // pin 3



// pour module LED 7 segments
#define  LED_PIN_CLK  2    // CLK pin  pour LED GND
#define  LED_PIN_DIO  3    // DIO pin  pour LED GND

// démarrage manuel
#define MODE_MANUEL  4        // démarrage manuel

// voyant lumineux
// OUTIL 1, OUTIL 2, OUTIL 3
byte Affiche[] = {5, 6, 7};

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
boolean detectionPouvoir[NOMBRE_OUTILS] = {0, 0, 0};
boolean collecteurEstOn = 0;

//Temporisation du collecteur de poussiere
int DC_spindown = 5000;
int DC_spinUP = 10;

// niveau de démarrage de l'outil
double ampThreshold = 2.50;


// pour chase light indicateur démarrage manuel
int ledDelay(65);
int direction = 1;
int currentLed;
unsigned long changeTime;

// instance du module LED 7 segments
TM1637   tm(LED_PIN_CLK, LED_PIN_DIO);

// Creer les instances du moniteur de courant EnergyMonitor
EnergyMonitor emon0;
EnergyMonitor emon1;
EnergyMonitor emon2;
