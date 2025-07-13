Projeto MONIT-PREV-CC2
Este repositório reúne os principais ficheiros e códigos desenvolvidos no âmbito do projeto MONIT-PREV-CC2, cujo objetivo é a monitorização e previsão do consumo de utilities, como água, gás e energia elétrica no Campus 2 do Instituto Politécnico de Leiria.

📟 Fase 1 — Monitorização com Arduino
Inicialmente, foi desenvolvida uma solução baseada em Arduino, tanto com o Arduino Uno quanto com o Arduino Mega.
Todo o projeto desta fase encontra-se na pasta Arduino_com_sensor_de_pulsos, com o código escrito na IDE Arduino.
A monitorização era feita através de sensores de pulsos ligados diretamente ao contador físico.

📷 Fase 2 — Monitorização por Imagem com ESP32-CAM
Numa segunda abordagem, explorou-se o uso de visão computacional para aferir o consumo a partir de imagens dos contadores analógicos:

O ficheiro Segmentação_imagens.py, escrito em Python, foi utilizado para testar métodos de segmentação de ponteiros em imagens dos contadores, destacando-os da imagem de fundo.

A pasta ESP32CAM_to_Drive contém o código (desenvolvido também na IDE Arduino) para o ESP32-CAM, que capta imagens periodicamente e as envia para o Google Drive, permitindo sua posterior análise.

O ficheiro Processamento_imagens_ESP32CAM.txt apresenta um excerto de um código mais avançado desenvolvido para o ESP32-CAM, capaz de capturar a imagem, realizar a segmentação e identificar os ponteiros automaticamente — possibilitando assim uma estimativa direta do consumo com o próprio microcontrolador.

🔄 Fase 3 — Contador Universal: Sistema Integrado e Escalável
Posteriormente, foi desenvolvida uma solução mais robusta e modular, denominada Contador Universal. Esta arquitetura contempla um ecossistema de microcontroladores que trabalham em conjunto:

A pasta Contador Universal contém subpastas para os diferentes dispositivos envolvidos:

TTGO LoRa32: lê os valores dos sensores de pulsos, armazena localmente e transmite os dados via LoRa.

LilyGo: recebe os dados via LoRa e os envia para uma base de dados localizada num Raspberry Pi.

ESP32-CAM: acionado pelo TTGO, captura imagens periodicamente como forma de redundância no processo de monitorização.

📊 Análise e Previsão de Consumos
Com os dados recolhidos, foram realizadas análises estatísticas e previsões utilizando ferramentas de machine learning e modelos de séries temporais:

Analisador_serie.py: script em Python que carrega séries temporais a partir de ficheiros Excel, imprime métricas descritivas, avalia correlação e estacionaridade, e gera gráficos para visualização.

Estudo_modelos_previsao.py: script responsável por testar diferentes modelos preditivos e identificar o que melhor se ajusta à previsão do consumo de determinada utility.

## 📜 Licença

Este repositório está licenciado sob os termos da **Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)**.  
Pode ser utilizado livremente para fins **não comerciais**, desde que seja dada a **devida atribuição** ao autor.  
Para mais detalhes, consulte o ficheiro [`LICENSE`](./LICENSE) ou visite [https://creativecommons.org/licenses/by-nc/4.0/](https://creativecommons.org/licenses/by-nc/4.0/).

