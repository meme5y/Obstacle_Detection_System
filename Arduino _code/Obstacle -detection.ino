const int trigPin = 2;
const int echoPin = 3;
const int buzzerPin = 8;
const int irSensorPin = 4;
const int redPin = 9;
const int greenPin = 10;
const int bluePin = 11;

int trainingData[SAMPLE_SIZE];
int trainingLabels[SAMPLE_SIZE];
int dataIndex = 0;
bool modelTrained = false;

float mlWeights[] = {0.15, 0.13, 0.12, 0.11, 0.10, 0.09, 0.08, 0.07, 0.06, 0.05, 0.04};
int mlBuffer[11];
int mlIndex = 0;

int distance;
int safetyDistance = 5;
int mlConfidence = 0;
bool mlAlert = false;

void setup() {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(irSensorPin, INPUT);
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  
  Serial.begin(9600);
  
  for(int i = 0; i < 11; i++) {
    mlBuffer[i] = safetyDistance + 10;
  }
  
  Serial.println("=== SISTEMA ML INICIADO ===");
  Serial.println("Fase de aprendizado: 20 segundos");
  setColor(255, 255, 0); // Amarelo - aprendizado
}

void loop() {
  distance = getUltrasonicDistance();
  int irValue = digitalRead(irSensorPin);
  
  mlBuffer[mlIndex] = distance;
  mlIndex = (mlIndex + 1) % 11;

  if (millis() < 20000) {
    learningPhase(distance, irValue);
  } else {
    operationalPhase(distance, irValue);
  }
  
  delay(100);
}

void learningPhase(int dist, int ir) {
  if (dataIndex < SAMPLE_SIZE) {
    trainingData[dataIndex] = dist;
    
    bool isAlert = (dist <= safetyDistance) || (ir == LOW);
    trainingLabels[dataIndex] = isAlert ? 1 : 0;
    
    dataIndex++;
    
    setColor(255, 255, 0);
    delay(50);
    setColor(0, 0, 0);
    delay(50);
    
    Serial.print("Aprendendo: ");
    Serial.print(dataIndex);
    Serial.print("/");
    Serial.print(SAMPLE_SIZE);
    Serial.print(" - Dist: ");
    Serial.print(dist);
    Serial.print(" - Label: ");
    Serial.println(trainingLabels[dataIndex-1]);
  }
  
  if (dataIndex == SAMPLE_SIZE && !modelTrained) {
    trainSimpleModel();
    modelTrained = true;
    Serial.println("=== MODELO TREINADO ===");
    setColor(0, 255, 0);
    delay(1000);
  }
}

void operationalPhase(int dist, int ir) {
  float mlPrediction = predictWithML();
  bool traditionalAlert = (dist <= safetyDistance) || (ir == LOW);
  
  bool finalAlert = traditionalAlert || (mlPrediction > 0.7);
  mlConfidence = mlPrediction * 100;
  
  if (finalAlert) {
    if (mlPrediction > 0.8) {
      highConfidenceAlert();
    } else {
      mediumConfidenceAlert();
    }
  } else {
    systemNormal();
  }
  
  onlineLearning(dist, ir);
  
  Serial.print("Dist: ");
  Serial.print(dist);
  Serial.print(" | ML: ");
  Serial.print(mlPrediction);
  Serial.print(" | Conf: ");
  Serial.print(mlConfidence);
  Serial.print("% | Alert: ");
  Serial.println(finalAlert ? "SIM" : "NÃO");
}

float predictWithML() {
  // 1. Média ponderada dos últimos valores
  float weightedAvg = 0;
  for(int i = 0; i < 11; i++) {
    weightedAvg += mlBuffer[i] * mlWeights[i];
  }
  
  float trend = 0;
  for(int i = 1; i < 11; i++) {
    trend += (mlBuffer[i] - mlBuffer[i-1]);
  }
  trend /= 10.0;
  
  float current = mlBuffer[(mlIndex + 10) % 11];
  float anomalyScore = 0;
  
  if (abs(current - weightedAvg) > 10) {
    anomalyScore = 0.8;
  }
  else if (trend < -2) {
    anomalyScore = 0.6;
  }
  else if (calculateOscillation() > 5) {
    anomalyScore = 0.4;
  }
  
  return anomalyScore;
}

float calculateOscillation() {
  float oscillation = 0;
  for(int i = 1; i < 11; i++) {
    oscillation += abs(mlBuffer[i] - mlBuffer[i-1]);
  }
  return oscillation / 10.0;
}

void trainSimpleModel() {
  Serial.println("Treinando modelo simples...");
  
  int alertCount = 0;
  for(int i = 0; i < SAMPLE_SIZE; i++) {
    if (trainingLabels[i] == 1) alertCount++;
  }
  
  float alertRatio = (float)alertCount / SAMPLE_SIZE;
  Serial.print("Ratio de alertas no treino: ");
  Serial.println(alertRatio);
}

void onlineLearning(int dist, int ir) {
  static unsigned long lastLearning = 0;
  
  if (millis() - lastLearning > 5000) { // A cada 5 segundos
    bool actualAlert = (dist <= safetyDistance) || (ir == LOW);
    float prediction = predictWithML();
    
    if ((prediction > 0.5 && !actualAlert) || (prediction < 0.3 && actualAlert)) {
      // Pequeno ajuste nos pesos
      for(int i = 0; i < 11; i++) {
        mlWeights[i] += (actualAlert ? 0.001 : -0.001);
      }
      normalizeWeights();
      
      Serial.println("Ajuste online nos pesos do ML");
    }
    
    lastLearning = millis();
  }
}

void normalizeWeights() {
  float sum = 0;
  for(int i = 0; i < 11; i++) {
    sum += mlWeights[i];
  }
  for(int i = 0; i < 11; i++) {
    mlWeights[i] /= sum;
  }
}

void highConfidenceAlert() {
  setColor(255, 0, 0);
  tone(buzzerPin, 1500, 200);
  delay(150);
  noTone(buzzerPin);
  delay(50);
}

void mediumConfidenceAlert() {
  setColor(255, 100, 0);
  tone(buzzerPin, 1000, 100);
  delay(300);
  noTone(buzzerPin);
  delay(200);
}

void systemNormal() {
  setColor(0, 255, 0);
  digitalWrite(buzzerPin, LOW);
}

int getUltrasonicDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  long duration = pulseIn(echoPin, HIGH);
  return duration * 0.034 / 2;
}

void setColor(int red, int green, int blue) {
  analogWrite(redPin, 255 - red);
  analogWrite(greenPin, 255 - green);
  analogWrite(bluePin, 255 - blue);
}
