# PSI-Microcontroladores2-Aula12
Atividade: Sincronismo, Detecção de Colisão e Integridade


## Etapa 1: Modelagem e Planejamento de Testes

Considerando o cenário proposto de comunicação entre duas placas com modo de operação simples de 5 segundos para transmitir e 5 segundos para receber, é natural que ocorram problemas de sincronismo: uma placa pode acabar transmitindo enquanto a outra está transmitindo também, e mesmo no recebimento podemos não receber a mensagem completa.

### 1.1. Sincronismo por Botão

O diagrama de estados abaixo foi elaborado para demonstrar o funcionamento da passagem de estado de cada microcontrolador:

Diagrama: <img width="1148" height="873" alt="image" src="https://github.com/user-attachments/assets/c26b20c8-f16e-42be-8c90-c646592f75de" />



Apartir desse planejamento, espera-se que as placas consigam não sofrer colisão e funcionar de maneira síncrona, sem necessitar de um botão que se pressione externamente, já que ela sempre checa a sincronização de ambas as placas na passagem de estados apartir do sinal que a placa mestre envia pelo botão.

Para verificar o funcionamento correto do código, testou-se o sincronismo entre as placas utilizando um funcionamento por LEDs, onde o LED azul indica que a placa está apenas com o TX enviando a mensagem enquanto o LED verde indica que a placa está apenas com o RX ativo e está recebendo as mensagens. Elaborando esse teste como TDD, deseja-se testar se as placas estão sincronizadas. Primeiramente, o código falha se qualquer um dos leds não for diferente do LED da outra placa à qualquer momento, já que elas nunca podem estar no mesmo estado ao mesmo tempo.  

| | Teste de Sincronia |
| ----- | ----|
| Pré-Condição | Ambas as placas começam em estados diferentes |
| Etapas de Teste | No momento que qualquer uma das placas trocar de estado, resetar uma placa. |
| Pós-Condição | A placa precisa estar no LED oposto ao LED da outra placa (Nesse caso, se uma placa estiver verde, a outra precisa estar azul e vice-versa. |

### 1.2. Detecção de Colisão

  Sobre a detecção de colisão, não seria necessária essa detecção pelo fato de nossa implemetação com botão não necessitar de uma pessoa externa para sincronizar. Isso faz com que o sincronismo seja automático
  e consiga coordenar sempre o estado do funcionamento de cada placa. Mas, apesar da nossa implementação não necessitar de detector de colisões, fez-se um código para detectar colisões e o diagrama de estados para
  demonstrar como as placas devem funcionar para saber se devem falar ou não.

  O diagrama de estados está logo abaixo para mostrar quais devem ser os estados e condições que a placa deve seguir para garantir a detecção por colisões:

 <img width="1576" height="1432" alt="image" src="https://github.com/user-attachments/assets/ea261e2d-9783-401c-bede-0a48ab551a83" />


  Para verificar o funcionamento correto do código, elaborou-se um teste que verifica se ele está realmente tratando as colisões. Nesse teste, as placas precisam verificar se outra placa está em TX/ se o canal está ocupado, ou seja, se a outra placa está enviando uma mensagem somente depois de verificar que o canal está aberto, a placa muda seu estado para TX para enviar mensagens.

  | | Teste de Colisão |
| ----- | ----|
| Pré-Condição | Ambas as placas começam em estados diferentes e elas não estão mais conectadas por botão (fio laranja) |
| Etapas de Teste: 1 | No momento que qualquer uma das placas der o tempo de trocar de estado, ela deve ficar em RX inicialmente|
| Etapas de Teste: 2 |Ela deve verificar o canal para ver se está recebendo uma mensagem. Se a placa estiver recebendo mensagem, ela permanece em RX e verfica novamente em 100ms. Se não, ela muda para TX e envia mensagens |
| Pós-Condição | Nenhuma das placas pode estar em estado de TX (nesse caso no Led Azul) ao mesmo tempo |


### 1.3. Verificação de Integridade

  Para a verificação de Integridade, foi vista a necessidade de implementar um verificador de tamanho e conteúdo para os pacotes que são enviados. O verificador de tamanho seerve como um teste rápido para ver a integridade da mensagem, enquanto que o verificador de conteúdo existe pois há a chance de a mensagem pode ter o mesmo tamanho, mas seu conteúdo estar alterado. Por isso, a verificação de integridade de conteúdo também deve saber o conteúdo da mensagem que recebe.

  O diagrama atualizado, envolvendo a verificação de integridade, o tratamento de colisões e a sincronia por botão, segue abaixo:

<img width="2075" height="1778" alt="image" src="https://github.com/user-attachments/assets/cd4d2c81-bfca-4b94-b9a8-7ce8f065295d" />

  | | Teste de Verificaçãp de Integridade |
| ----- | ----|
| Pré-Condição | Ambas as placas começam em estados diferentes e uma placa envia o pacote de forma errada para a outra placa|
| Etapas de Teste: 1 | Uma placa em estado de TX vai enviar um pacote propositalmente errado para RX da outra placa |
| Etapas de Teste: 2 | A outra placa vai ler o pacote  |
| Pós-Condição | A placa em estado de RX deve ascender o LED vermelho para sinalizar o erro no envio durante seu período de RX |

## Etapa 2: Desenvolvimento Orientado a Testes

A partir da modelagem realizada e dos testes planejados, faça o desenvolvimento da solução para contemplar os 3 requisitos e passar nos 3 testes descritos.

O uso de IA Generativa é incentivado: _veja a diferença entre fazer prompts sem fornecer os requisitos e testes planejados, ou usar prompts com os diagramas e testes planejados_.

Além dos testes de cada requisito em cada etapa, faça **testes de regressão** também, para garantir que os requisitos das etapas anteriores estão funcionando (Dica: podemos ter modos de operação diferentes para testar diferentes features e não nos confundirmos com os comportamentos dos leds em cada situação).
Isto é: se o sincronismo continua funcionando após a integração da detecção de colisão, e se o sincronismo e a detecção continuam funcionando após a adição da verificação de integridade.

_Faça o upload de todos os códigos no repositório_ (pode ser em branches diferentes, ou até organizar em pull requests as diferentes features).

_Vocês devem adicionar todas as evidências de funcionamento (como por exemplo capturas de tela e fotos) dos testes realizados, mostrando todos os testes realizados no README.
As imagens e outras evidências de funcionamento devem estar descritas no README e devem estar em uma pasta chamada "results" no repositório._

### 2.1. Sincronismo por Botão

Testou-se se as placas sincronizavam mesmo resetando elas em tempos diferentes. Os testes podem ser vistos no vídeo a seguir:


https://github.com/user-attachments/assets/4d33eeb9-d415-47ee-8040-3090cb11e4ef

### 2.2. Detecção de Colisão

Testou-se a realização do tratamento de colisão sem o botão (utilizado pelo fio laranja, ou seja, sem o fio laranja) das placas e se elas sincronizavam mesmo sem o botão: 

https://github.com/user-attachments/assets/87f40b9a-f888-48fc-b17b-eca830df5583


### 2.3. Verificação de Integridade

Testou-se a verificação de integridade das mensagens das placas. No caso, se a placa verificar que existe um erro na mensagem, ela liga o LED vermelho durante o período em que está no modo RX. Nesse vídeo, uma placa foi configurada para enviar o pacote com a payload "paralelepipeda" enquanto a outra espera receber o pacote "paralelepipedo" :


https://github.com/user-attachments/assets/c4693a26-e9d9-4244-8f3c-15e218445e88


Para o propósito de comparação com o caso de funcionamento normal, as duas placas estão configuradas com o mesmo pacote secreto, que elas precisam receber certo:

https://github.com/user-attachments/assets/a7d73e08-ab6a-4d11-8bf1-683c6210c617

### 2.4 Chat

