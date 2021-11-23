#include <FirebaseArduino.h>
#include <ESP8266WiFi.h>
#include <Servo.h>

#define FIREBASE_HOST "PROJETO.firebaseio.com" // url do firebase
#define FIREBASE_AUTH "TOKEN" // chave do firebase para ter o acesso
#define WIFI_SSID "REDE" // nome do wifi que o node vai conectar
#define WIFI_PASSWORD "SENHA" // senha do wifi

bool sensorFD; // sensor fora direita (fora da linha)   pino D0
bool sensorD; // sensor direita    pino D1
bool sensorE; // sensor esquerda   pino D2
bool sensorFE; // sensor fora esquerda (fora da linha)   pino D3

bool VsensorFD; // V de velho, essas variaveis foram usadas para guardarem o estado antigo das variaveis correspondentes
bool VsensorD;
bool VsensorE;
bool VsensorFE;

bool carga; // botoes para saber se a caixa esta no robo   pino SD2

bool x;

String direcao; // usada para guardar a informacao da direcao que o robo deve seguir, Ex. "direita"
short opcao; // opcao selecionada pelo usuario

long tempo;

Servo servo1; // cria uma nova instancia com o nome de servo1


void setup() {
  pinMode(D0, INPUT); // sensor infravermelho FD
  pinMode(D1, INPUT); // D
  pinMode(D2, INPUT); // E
  pinMode(D5, INPUT); // FE

  pinMode(9, INPUT); // carga (botoes conectados em serie)

  pinMode(D4, OUTPUT); // motor da esquerda
  pinMode(D6, OUTPUT);

  pinMode(D7, OUTPUT); // motor da direita
  pinMode(D8, OUTPUT);

  Serial.begin(9600);

  servo1.attach(D3); // indica que o servo1 esta usando o pino D3

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); // inicia a conecao wifi com o nome e senha da rede
  Serial.print("conectando");
  while(WiFi.status() != WL_CONNECTED){ // enquando o node conecta ele print "." no serial a cada meio segundo
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("conectado a: ");
  Serial.println(WiFi.localIP()); // printa o endereco IP da rede local

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH); // inicia a conecao com o servidor do firebase
}


void loop() { // ler as funcoes antes do loop, ajudara no entendimento do codigo(linha 195)
  lerSensores();
  servo1.write(0);
  opcao = 0; 
  x = 0;
  mover("parado"); // reseta as variaveis para que a ultima volta nao afete a proxima
  
  while(opcao == 0){ // enquanto o node nao receber um pedido ele ira esperar parado
    evitarReset();

    if(Firebase.failed()){ // caso a conecao com o fire base caia ele tenta reconectar
      delay(0);
      Serial.print("setting number failed:");
      Serial.println(Firebase.error()); // printa no serial qual erro ocorreu
      firebasereconnect(); // usa a funcao de reconectar
      return;
    }

    opcao = Firebase.getString("Carga").toInt(); // pega a opcao que esta no firebase, se ninguem fizer o pedido, ela ficara como 0

    Serial.println("esperando...");
    delay(1000);
  }

  Firebase.setInt("Carga", 0); // reseta a variavel do firebase para 0 para que outra pessoa possa pedir outra carga
  
  lerSensores();

  while(sensorFE == HIGH || sensorFD == HIGH){ // caso os sensores estejam em cima da linha (e nao com a linha entre eles) segue reto
    evitarReset();
    lerSensores();
    mover("frente");    
  }
  delay(200);
  
  moverNaLinha(); // faz o robo seguir reto ate bater na primeira bifurcacao
  virarDireita(); // faz o robo virar a direita para seguir em direcao ao armazem
  moverNaLinha(); // continua a seguir a linha
  virarEsquerda(); // quando bate na linha ele vira a esquerda e chega no armazem
  moverNaLinha(); // continua na linha

  while(true){ // faz com que o robo chegue ate a carga correspondente, passando pelas que nao foram escolhidas
    evitarReset();
        
    while(sensorFE == 0 && sensorFD == 1){ // caso o sensor detecte que ha uma carga a direita, entra no while
      evitarReset();
      lerSensores();
      
      if(opcao == 1){ // se a opcao for 1, entao o robo deve virar a direita para chegar ate a carga escolhida
        virarDireita(); // ele vira a direita
        break; // e sai do while e continua no outro while
      }else{
        mover("frente"); // se a carga estiver mais a frente, segue em frente
        x = 1;
      }
      delay(100); // espera 100ms para que os sensores saiam de cima da linha
    }

    if(opcao == 1){ // confere se esta na carga certo e sai do while e continua no void loop
      break;
    }

    if(x == 1){ // caso tenha detectado uma carga a direita porem nao era o escolhida, diminuira a opcao em 1 ate que chegue a carga escolhida
      opcao--;
      x = 0;
    }

    moverNaLinha(); // continua seguindo reto ate encontrar outra entrada para uma nova carga
  }

  moverNaLinha(); // segue reto

  mover("tras"); // neste ponto o robo passou debaixo da caixa e chegou no limite, entao ele comeca a dar meia volta
  delay(75);
  mover("parado");

  meiaVolta();

  servo1.write(90); // quando termina de dar meia volta, ele sobe o eixo do motor servo para que empurre a carga em cima dele
  delay(400);

  moverNaLinha(); // continua movendo em frente ate que voltei para o "corredor" do armazem
  virarEsquerda(); // faz com que vire a esquerda em direcao ao usuario

  while(sensorFD == LOW){ // faz que o robo siga em frente mesmo que o sensorFE seje ativado, quando o sensorFD for ativado significa quem o robo finalmente saiu do armazem
    evitarReset();
    lerSensores();
    moverNaLinha();
    mover("frente"); // quando o robo sair da funcao moverNaLinha() e porque o sensorFE esta em cima da linha, para tira-lo o robo segue em frente por uns milisegundos
    delay(50);
  }
  virarDireita(); // vira a direira e comeca a seguir em frente
  moverNaLinha();
  mover("frente"); // ele batei no local de descanso, entao ele segue enfrente
  
  while(sensorFE == HIGH || sensorFD == HIGH){ // ate que os sensores detectem que ele passou do local de descanso
    evitarReset();
    lerSensores();
  }
  
  moverNaLinha(); // depois disso ele apenas segue a linha, chegando ate o trabalhador que pediu a carga
  
  mover("tras"); // evita que a velocidade do carrinho nao faca com que ele saia do limite da linha
  delay(75);
  mover("parado");

  while(carga == 1){ // esse while faz com que o robo fique parado esperando que a carga seja retirada de cima dele
    evitarReset();
    lerSensores();
    delay(1000); // faz com que verifica o estado da carga apenas uma vez por segundo
  }

  delay(2000); // espera 2 segundos para que o trabalhador tenha tempo o suficiente de sair da frente

  meiaVolta(); // da meia volta
  moverNaLinha(); // segue na linha
  virarDireita(); // quando bater na zona de descanso
  moverNaLinha(); // entra nela
  meiaVolta(); // quando bater no limite dela, da meia volta e fica na mesma posicao, apenas esperando o proximo pedido
}

/*
 * Funcoes no C funcionam que nem as do Javascript, sempre que voce chamar a funcao, Ex. "moverNaLinha()"
 * ele comecara a executar o codigo dentro dela, tirando a necessidade de colocar o mesmo codigo mais
 * de uma vez, se estao definidas com "void" antes, elas nao retornam nada.
 */


void firebasereconnect(){ // esta funcao e usada no comeco do loop para tentar reconectar o node caso a conecao seja interrompida com o firebase
  Serial.println("trying to reconnect");
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH); // usa a url e a chave do firebase definidas no inicio do codigo
}



void virarDireita(){ // esta funcao serve para que o robo vire 90 graus a direita

  while(sensorFD == HIGH){ // faz com que o robo saia da faixa preta para nao haver problemas na hora de virar
    lerSensores();
    mover("frente"); // ele sai seguindo em frente ate que o sensor FD pare de detectar a linha
  }

  delay(80); // espera um pouco para nao haver problemas para virar

  mover("direita"); // comeca a virar a direita

  while(true){
    evitarReset();
    lerSensores();

    if(sensorD != VsensorD && sensorD == LOW){ // O robo sabe a hora de parar quando o sensorD passar de 1 para 0, indicando que ele cruzou a linha e ela esta entre o sensorD e o sensorE
      mover("esquerda"); // ele move a esquerda por 25ms para que o a velocidade do robo nao faca com que saia da linha
      delay(25);
      VsensorD = sensorD; // guarda a ultima posicao no sensorD para a proxima vez que o robo virar a direita
      break; // sai do while e volta para o void loop
    }

    VsensorD = sensorD;// guarda a ultima posicao no sensorD para ver se o sensor passou de 1 para 0
  }
}


void virarEsquerda(){ // quase igual a ultima funcao, porem usa o sensorE e move para a esquerda
  while(sensorFE == HIGH){
    lerSensores();
    mover("frente");
  }

  delay(80);
  
  mover("esquerda");

  while(true){
    evitarReset();
    lerSensores();

    if(sensorE != VsensorE && sensorE == LOW){ // tive q coloca entre ler os sensores e joga a variavel no VsensorD
      mover("direita");
      delay(25);
      VsensorE = sensorE;
      break;
    }

    VsensorE = sensorE;
  }
}

void meiaVolta(){ // faz com que o robo vire 180 graus
  mover("esquerda"); // comeca a mover para a esquerda
  delay(500);
  while(sensorE == LOW){ // continua virando a esquerda ate que a linha bata no sensorE
    evitarReset();
    lerSensores();
  }
  mover("direita"); // apos bater ele vira a direita por 50ms para que a velocidade acumulada nao jogue o carrinho para fora da linha
  delay(50);
  mover("parado"); // faz com que o robo pare de andar
}


void evitarReset(){ // foi pesquisado na internet o motivo do "soft reset" e a solucao foi executar delay(0); uma vez por segundo em todo while
  if ((millis() - tempo) > 1000) { // se passar mais de 1 segundo desde da ultima vez que rodou, ira rodar de novo
    delay(0);
    tempo = millis();
  }
}


void lerSensores(){ // essa funcao e usada para ler os sensores
  sensorFD = digitalRead(D0); // o sensor ve 1 para preto e 0 para branco
  sensorD = digitalRead(D1);
  sensorE = digitalRead(D2);
  sensorFE = digitalRead(D5);

  carga = digitalRead(9);
}


void direcaoLinha(){ // essa funcao e usada para saber qual direcao o robo tem que ir para que nao saia da linha
  if(sensorE == 0 && sensorD == 0){
    direcao = "frente";
  }else if(sensorE == 1 && sensorD == 0){
    direcao = "esquerda frente";
  }else if(sensorE == 0 && sensorD == 1){
    direcao = "direita frente";
  }else if(sensorE == 1 && sensorD == 1){
    direcao = "parado";
  }
}


void moverNaLinha(){ // faz o robo seguir a linha, juntando as algumas funcoes para que so haja necessidade de colocar essa no void loop
  lerSensores();
  while(sensorFE == LOW && sensorFD == LOW){ // caso um dos sensores de fora detecte alguma linha, essa funcao para de rodar
    evitarReset();
    lerSensores();
    direcaoLinha(); // pega a direcao que o robo tem que seguir
    mover(direcao); // e manda o robo seguir ela
  }
  mover("frente");
  delay(80);
}


void mover(String direcao){ // D4 high e D5 low vai pra frente, oposto vai para tras, os 2 low fica parado
  if(direcao == "frente"){ // converte a variavel direcao de string para movimento do carrinho
    digitalWrite(D4, HIGH);
    digitalWrite(D6, LOW);

    digitalWrite(D7, HIGH);
    digitalWrite(D8, LOW);
    
  }else if(direcao == "tras"){
    digitalWrite(D4, LOW);
    digitalWrite(D6, HIGH);

    digitalWrite(D7, LOW);
    digitalWrite(D8, HIGH);
  }else if(direcao == "esquerda frente"){
    digitalWrite(D4, LOW);
    digitalWrite(D6, LOW);

    digitalWrite(D7, HIGH);
    digitalWrite(D8, LOW);
  }else if(direcao == "esquerda tras"){
    digitalWrite(D4, LOW);
    digitalWrite(D6, LOW);

    digitalWrite(D7, LOW);
    digitalWrite(D8, HIGH);
  }else if(direcao == "esquerda"){
    digitalWrite(D4, LOW);
    digitalWrite(D6, HIGH);

    digitalWrite(D7, HIGH);
    digitalWrite(D8, LOW);
    
  }else if(direcao == "direita frente"){
    digitalWrite(D4, HIGH);
    digitalWrite(D6, LOW);

    digitalWrite(D7, LOW);
    digitalWrite(D8, LOW);
  }else if(direcao == "direita tras"){
    digitalWrite(D4, LOW);
    digitalWrite(D6, HIGH);

    digitalWrite(D7, LOW);
    digitalWrite(D8, LOW);
  }else if(direcao == "direita"){
    digitalWrite(D4, HIGH);
    digitalWrite(D6, LOW);

    digitalWrite(D7, LOW);
    digitalWrite(D8, HIGH);
    
  }else if(direcao == "parado"){
    digitalWrite(D4, LOW);
    digitalWrite(D6, LOW);

    digitalWrite(D7, LOW);
    digitalWrite(D8, LOW);
  }
} // agora leia o void loop
