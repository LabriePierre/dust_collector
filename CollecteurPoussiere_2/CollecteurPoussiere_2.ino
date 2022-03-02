// Code de contrôle d'aspirateur couplé à des outils tel un planeur et dégauchisseuse.
// Cumule les nombre d'heure de fonctionnement de chaque appareil.

// Pierre Labrie, 2022/01/28, Licence GNU GPL V3

#include "variables.h"      // librairie pour  Emon


void setup() {
  //initialisation des pins
  pinMode(DC_RELAIS_NO, OUTPUT);
  pinMode(DC_RELAIS_NC, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  pinMode(Affiche[0], OUTPUT);
  pinMode(Affiche[1], OUTPUT);
  pinMode(Affiche[2], OUTPUT);

  digitalWrite(DC_RELAIS_NO, HIGH);
  digitalWrite(DC_RELAIS_NC, HIGH);
  digitalWrite(LED_BUILTIN, LOW);

  //pins du sélecteur binaire
  pinMode(SELECT_0, INPUT_PULLUP);
  pinMode(SELECT_1, INPUT_PULLUP);
  pinMode(SELECT_2, INPUT_PULLUP);

  // Interrupteur du mode manuel
  pinMode(MODE_MANUEL, INPUT_PULLUP);



  tm.init();              // initialise l'affichage LED 7 segments
  tm.set(2);              // ajuste la luminosité (1 à 7)
  afficheNombre(8888);    // Affiche 8888 pour validation de tous les segments


  Serial.begin(9600);
  while (!Serial) ; // attente de l'ouverture du port série Arduino

  //  remise à zéro du compteur pour montage initial
  //  ecritDansEEPROM(100, minutesTotales[0]);
  //  ecritDansEEPROM(200, minutesTotales[0]);
  //  ecritDansEEPROM(300, minutesTotales[0]);

  // lecture du dernier état avant une panne électrique
  // réinitialise les variables globales
  minutesTotales[0] = lectureDeEEPROM(100);
  minutesTotales[1] = lectureDeEEPROM(200);
  minutesTotales[2] = lectureDeEEPROM(300);



  // configuration Emon pour module Hall SCT 013-30
  // Current: input pin, calibration.
  emon0.current(PIN_LECTURE_0, 60);
  emon1.current(PIN_LECTURE_1, 60);
  emon2.current(PIN_LECTURE_2, 60);

  // pour stabilisation du voltage
  delay(5000);
}

void loop() {


  //verifie le fonctionnement de tous les outils
  bool outilActif[NOMBRE_OUTILS] = {0, 0, 0};

  bool interrupManuel  = verif_Interrup(MODE_MANUEL);

  for (int i = 0; i < NOMBRE_OUTILS; i++) {
    if (verifieChangeCourant(i)) {
      outilActif[i] = true;

      if (detectionPouvoir[i]  == false) {
        tempsEcoule[i] = millis();
      }
      detectionPouvoir[i]  = true;
      lumieresTemoins(true, i);
    }
  }
  
  chase(interrupManuel,outilActif);

  
  //démarre l'aspirateur selon l'état de n'importe quel outil ou du démarrage manuel.
  if (outilActif[0] || outilActif[1] || outilActif[2] || interrupManuel ) {

    if (collecteurEstOn == false) {
      delay(DC_spinUP);
      ouvreCollecteurPoussiere();
    }
  }
  else
  {
    if (collecteurEstOn == true) {
      delay(DC_spindown);
      fermeCollecteurPoussiere();
    }
  }

  //calcul du temps écoulé par outil et cumule les minutes totales dans le registre
  for (int i = 0; i < NOMBRE_OUTILS; i++) {

    if (detectionPouvoir[i] == true && outilActif[i] == false ) {

      calculTempsOutil(i);
      detectionPouvoir[i] = false;
      lumieresTemoins(false, i);
      cumulTempsOutil(i);
    }
  }

  //debug();

  // Affiche les heures sur led selon le sélecteur
  AfficheSelection(analyseSelecteurBinaire());

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


// AFFICHE[] sur le LED la valeur en minutes ou heures pour l'outil sélectionné
void AfficheSelection(int selection)
{
  switch (selection) {

    case 0:
      afficheNombre(8888);
      break;
    case 1:
      afficheNombre(minutesTotales[0] / 60);
      break;
    case 2:
      afficheNombre(minutesTotales[1] / 60);
      break;
    case 3:
      afficheNombre(minutesTotales[2] / 60);
      break;
    case 4:
      afficheNombre(minutesTotales[0]);
      break;
    case 5:
      afficheNombre(minutesTotales[1]);
      break;
    case 6:
      afficheNombre(minutesTotales[2]);
      break;

    default:
      afficheNombre(8888);
  }

}

//affichage du nombre de minutes ou heures dans le led 7 segments
// supporte un nombre de plus de 4 chiffres en omettant la partie de gauche
void afficheNombre(long numNouv) {

  tm.display(3, numNouv % 10);
  tm.display(2, numNouv / 10 % 10);
  tm.display(1, numNouv / 100 % 10);
  tm.display(0, numNouv / 1000 % 10);
}


// Vérification du passage de courant dans le module Hall
// Vérifie un des trois outils prévus
// méthode privée
boolean _verifieChangeCourant(int outil) {

  double Irms = 0;
  // Calculer Irms seulement
  switch (outil) {

    case 0:
      Irms = emon0.calcIrms(465); // basé sur 5 échantillons
      break;

    case 1:
      Irms = emon1.calcIrms(465);
      break;

    case 2:
      Irms = emon2.calcIrms(465);
      break;

    default:
      Irms = 0;
  }

  // pour debug
  //          Serial.print("outil: ");
  //          Serial.println(outil);
  //          Serial.print(Irms);
  //          Serial.println(" Amps RMS");


  // si le courant dépasse le niveau minimum, retourne vrai
  if (Irms > ampThreshold) {
    return true;
  } else {
    return false;
  }
}

// Vérification du passage de courant dans le module Hall
// méthode publique
// Assure un réél démarrage pour éviter les interférences des connecteurs (debounce)
boolean verifieChangeCourant(int outil) {
  int intDelay = 10;

  bool blnBtn_A = _verifieChangeCourant(outil);
  delay (intDelay);

  bool blnBtn_B  = _verifieChangeCourant(outil);

  if (blnBtn_A == blnBtn_B)
  {
    delay( intDelay);
    bool blnBtn_C  = _verifieChangeCourant(outil);
    if (blnBtn_A == blnBtn_C)
      return blnBtn_A;
  }

  return false;
}


// Ouvre aspirateur
// contact momentané seulement
void ouvreCollecteurPoussiere() {
  Serial.println("ouvreCollecteurPoussiere");
  digitalWrite(DC_RELAIS_NO, LOW);
  delay(1500);
  digitalWrite(DC_RELAIS_NO, HIGH);
  digitalWrite(LED_BUILTIN, HIGH);
  collecteurEstOn = true;
}

// Ferme aspirateur
// contact momentané seulement
void fermeCollecteurPoussiere() {
  Serial.println("fermeCollecteurPoussiere");
  digitalWrite(DC_RELAIS_NC, LOW);
  delay(1500);
  digitalWrite(DC_RELAIS_NC, HIGH);
  digitalWrite(LED_BUILTIN, LOW);
  collecteurEstOn = false;
}


// Calculer le temps d'opération écoulé
// Utilise la variable globale tempsEcoule initialisée à la détection de l'outil
void calculTempsOutil(int outil) {

  if (tempsEcoule[outil] > 0 )
  {
    secondes[outil] = (millis() - tempsEcoule[outil]) / 1000;
    minutes[outil] = secondes[outil] / 60;
    secondes[outil] %= 60;
  }
}



// Totalise le temps global d'opération
// Mémorise dans EEPROM le nombre global de minutes pour garder les données du compteur en cas de panne de courant.
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
          ecritDansEEPROM(100, minutesTotales[outil]);;
          break;

        case 1:
          ecritDansEEPROM(200, minutesTotales[outil]);;
          break;

        case 2:
          ecritDansEEPROM(300, minutesTotales[outil]);;
          break;
      }

    }

  }
}

//Écriture en mémoire du total d'heures cumulées
void ecritDansEEPROM(int address, long number)
{
  EEPROM.write(address, (number >> 24) & 0xFF);
  EEPROM.write(address + 1, (number >> 16) & 0xFF);
  EEPROM.write(address + 2, (number >> 8) & 0xFF);
  EEPROM.write(address + 3, number & 0xFF);
}



//Lecture de la mémoire du total d'heures cumulées
long lectureDeEEPROM(int address)
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
        digitalWrite(Affiche[0], HIGH);
      }
      else
      { digitalWrite(Affiche[0], LOW);
      }
      break;

    case 1:
      if (bouton) {
        digitalWrite(Affiche[1], HIGH);
      }
      else
      { digitalWrite(Affiche[1], LOW);
      }
      break;

    case 2:
      if (bouton) {
        digitalWrite(Affiche[2], HIGH);
      }
      else
      { digitalWrite(Affiche[2], LOW);
      }
      break;

  }
}

// Pour indicateur démarrage manuel
void chase(bool interrupManuel,bool outilActif[]) {

  if (interrupManuel) {

    if ((millis() - changeTime) > ledDelay)
    {
      changeTime = millis();
      changeLed();
    }

  }
  else
  {
    for ( int i = 0; i < NOMBRE_OUTILS; i++)
  {
   if(!outilActif[i]){
    digitalWrite(Affiche[i], LOW);
   }
  }
    }
}

//Effet de séquenceur de lumière
void changeLed()
{
  for ( int i = 0; i < NOMBRE_OUTILS; i++)
  {
    digitalWrite(Affiche[i], LOW);
  }
  
  digitalWrite(Affiche[currentLed], HIGH);
  currentLed += direction;
  if (currentLed == 2)
  {
    direction = -1;
  }
  if (currentLed == 0)
  {
    direction = 1;
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
