# Bibliotecas
import cv2
import numpy as np
import matplotlib.pyplot as plt
import math
from google.colab import drive

# Montar o Google Drive
drive.mount('/content/drive')

# Carregar a imagem do Google Drive
image_path = "/content/drive/My Drive/ESP32-CAM/20241031/20241031-123753.jpg"

#Abrir imagem do caminho dado
image = cv2.imread(image_path)

#Verifica se a imagem existe
if image is None:
    print("Erro ao carregar a imagem. Verifique o caminho do arquivo.")
else:
    # Recortar a regiao do numero central
    x, y, w, h = 570, 720, 250, 250
    central_number = image[y:y+h, x:x+w]

# Converter para RGB e HSV
imagem_rgb = cv2.cvtColor(central_number, cv2.COLOR_BGR2RGB)
imagem_hsv = cv2.cvtColor(central_number, cv2.COLOR_BGR2HSV)

# Definir intervalos para vermelho
limite_inferior_vermelho = np.array([0, 100, 100])
limite_superior_vermelho = np.array([29, 255, 255])

# Criar mascara
mascara_vermelho = cv2.inRange(imagem_hsv, limite_inferior_vermelho, limite_superior_vermelho)

# Destacar vermelho
imagem_destacada_vermelho = cv2.bitwise_and(imagem_rgb, imagem_rgb, mask=mascara_vermelho)

# Exibir imagens
plt.figure(figsize=(12, 6))
plt.subplot(1, 2, 1)
plt.imshow(imagem_rgb)
plt.title("Imagem Original")
plt.axis('off')
plt.subplot(1, 2, 2)
plt.imshow(imagem_destacada_vermelho)
plt.title("Imagem com Ponteiro Vermelho")
plt.axis('off')
plt.show()