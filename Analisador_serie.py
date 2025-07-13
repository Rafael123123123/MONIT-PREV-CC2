# Bibliotecas necessárias
import pandas as pd
import numpy as np
import seaborn as sns
import matplotlib.pyplot as plt
import statsmodels.api as sm
import warnings
from statsmodels.tsa.seasonal import seasonal_decompose
from statsmodels.tsa.seasonal import STL
import requests
import os

# Ignorar todos os warnings
warnings.filterwarnings("ignore")

# CONFIGURAÇÃO INICIAL - ALTERAR CONFORME NECESSÁRIO
name_arquivo = "AguaDiario.csv"
unidade_consumo = "L"  # Alterar conforme o tipo de utility (kWh, m³, etc.)
tipo = 1 # Tipo de arquivo: 1 para apenas hora 2 para data e hora

# Nome do ficheiro base (sem extensão)
nome_base = os.path.splitext(os.path.basename(name_arquivo))[0]

# Função auxiliar para guardar figuras
def save_fig(nome):
    plt.savefig(f"{nome_base}_{nome}.png", bbox_inches='tight')
    #plt.close()

# Abre o arquivo com os dados de consumo
fileCompleto = pd.read_csv(name_arquivo, delimiter=';', decimal=',')
#fileCompleto = pd.read_csv("ConsumoAguaSemanal.csv", delimiter=';', decimal=',')
#fileCompleto = pd.read_excel("Consumo total agua 5mins.xlsx", sheet_name="Consumo hora")

fileCompleto.columns = ['Data', 'Consumo']

if tipo == 1:
    # Converte a coluna 'Data' para datetime
    fileCompleto['Data'] = pd.to_datetime(fileCompleto['Data'], format='%d/%m/%Y', errors='coerce')
else:
    # Converte a coluna 'Data' para datetime
    fileCompleto['Data'] = pd.to_datetime(fileCompleto['Data'], format='%d/%m/%Y %H:%M:%S', errors='coerce')

# Remove linhas com valores NaN
fileCompleto.dropna(inplace=True)  

# Imprime o dataframe completo
plt.figure(figsize=(12, 6))
p = sns.lineplot(x= fileCompleto['Data'], y= fileCompleto['Consumo'], color='blue')
p.set_xticklabels(p.get_xticklabels(), rotation = 45, horizontalalignment = "right")
plt.ylabel(f"Consumo ({unidade_consumo})")
save_fig("DadosTotaisConsumo")
plt.show()

# Verifica se o ano de 2023 existe nos dados
if 2023 in fileCompleto['Data'].dt.year.unique():
    # Cria um novo dataframe com os dados apenas de 2023
    file2023 = fileCompleto[fileCompleto['Data'].dt.year == 2023]

    # Imprime o dataframe apenas de 2023
    plt.figure(figsize=(12, 6))
    p = sns.lineplot(x=file2023['Data'], y=file2023['Consumo'], color='blue')
    p.set_xticklabels(p.get_xticklabels(), rotation=45, horizontalalignment="right")
    plt.ylabel(f"Consumo ({unidade_consumo})")
    save_fig("UltimoAnoConsumo")
    plt.show()
else:
    print("O ano de 2023 não existe nos dados.")

# Imprime o histograma do consumo
plt.figure(figsize=(10, 6))
fileCompleto['Consumo'].plot(kind='hist', bins=40, color='blue')
plt.xlabel(f"Consumo ({unidade_consumo})")
save_fig("HistogramaConsumo")
plt.show()

# Define a coluna 'Data' como índice
fileCompleto.set_index('Data', inplace=True)
# Resample para periodicidade semanal e diario somando os valores
fileSemanal = fileCompleto.resample('W').sum()
fileDiario = fileCompleto.resample('D').sum()
# Resetar o índice, para ter 'Data' como coluna novamente
fileCompleto.reset_index(inplace=True)
fileSemanal.reset_index(inplace=True)
fileDiario.reset_index(inplace=True)

# Adicionar a coluna 'nSemana' com o número da semana do ano
fileCompleto['nSemana'] = fileCompleto['Data'].dt.isocalendar().week
fileSemanal['nSemana'] = fileSemanal['Data'].dt.isocalendar().week

# Adicionar a coluna 'DiaSemana' e 'Mes' com o dia da semana e mês
fileCompleto['DiaSemana'] = fileCompleto['Data'].dt.day_name()
fileCompleto['Mes'] = fileCompleto['Data'].dt.month

# Mapear os dias da semana para português
mapeamento_dias = {
    'Monday': 'Segunda-feira',
    'Tuesday': 'Terça-feira',
    'Wednesday': 'Quarta-feira',
    'Thursday': 'Quinta-feira',
    'Friday': 'Sexta-feira',
    'Saturday': 'Sábado',
    'Sunday': 'Domingo'
}

fileCompleto['DiaSemana'] = fileCompleto['DiaSemana'].map(mapeamento_dias)

print("\n\n########### Estatisticas Gerias ###########\n")
# Calcular a autocorrelação para lag de 15
acf_values = sm.tsa.acf(fileCompleto['Consumo'], nlags=25) 

# Imprime o gráfico de autocorrelação
plt.figure(figsize=(10, 5))
plt.stem(range(len(acf_values)), acf_values)
plt.xlabel('Lag')
plt.ylabel('Autocorrelação')
save_fig("Autocorrelacao")
plt.show()

#cria uma tabela dinâmica para o consumo médio por dia da semana e semana do ano
tabela = fileCompleto.pivot_table(values='Consumo', index='DiaSemana', columns='nSemana', aggfunc='mean')

# Definir a ordem dos dias da semana
ordem_dias = ['Segunda-feira', 'Terça-feira', 'Quarta-feira', 'Quinta-feira', 'Sexta-feira', 'Sábado', 'Domingo']
tabela = tabela.reindex(ordem_dias)

# Imprime a tabela
plt.figure(figsize=(18,6))
sns.heatmap(tabela, cmap="YlOrRd", linewidths=0.3, linecolor='gray')
plt.xlabel('Semana do Ano')
plt.ylabel('Dia da Semana')
save_fig("ConsumoMedioDiaSemana")
plt.show()

# Adicionar gráfico boxplot por dia da semana
plt.figure(figsize=(12, 6))
# Preparar dados para boxplot por dia da semana
fileCompleto_plot = fileCompleto.copy()
fileCompleto_plot['DiaSemana'] = pd.Categorical(fileCompleto_plot['DiaSemana'], categories=ordem_dias, ordered=True)
sns.boxplot(data=fileCompleto_plot, x="DiaSemana", y="Consumo", palette="Blues")
plt.xlabel("Dia da Semana")
plt.ylabel(f"Consumo ({unidade_consumo})")
plt.xticks(rotation=45)
save_fig("BoxplotDiaSemana")
plt.show()

# Calcular estatísticas em função do número da semana
estatisticas_semanas = fileSemanal.groupby('nSemana')['Consumo'].agg(['mean', 'median', 'min', 'max', 'quantile'])

# Calcular os quartis (25% e 75%)
quartis = fileSemanal.groupby('nSemana')['Consumo'].quantile([0.25, 0.75]).unstack()
estatisticas_semanas['Q1'] = quartis[0.25]
estatisticas_semanas['Q3'] = quartis[0.75]

# Resetar o índice para melhor visualização
estatisticas_semanas.reset_index(inplace=True)

# Renomear colunas para portugues
estatisticas_semanas.rename(columns={
    'mean': 'Media',
    'median': 'Mediana',
    'min': 'Minimo',
    'max': 'Maximo',
}, inplace=True)

print("\n\n########### Estatisticas Semanais ###########\n")
# Exibir as estatísticas
print(estatisticas_semanas)

# Imprime as estatísticas em um gráfico
plt.figure(figsize=(10, 6))
plt.plot(estatisticas_semanas['nSemana'], estatisticas_semanas['Media'], label='Média', marker='1', color='blue')
plt.plot(estatisticas_semanas['nSemana'], estatisticas_semanas['Minimo'], label='Mínimo', marker='2', color='red')
plt.plot(estatisticas_semanas['nSemana'], estatisticas_semanas['Maximo'], label='Máximo', marker='3', color='green')
plt.xlabel('Número da Semana')
plt.ylabel(f'Consumo ({unidade_consumo})')
plt.grid(True)
plt.legend()
save_fig("EstatisticasSemanais")
plt.show()

# Imprime o gráfico de boxplot para visualizar a distribuição do consumo por semana
plt.figure(figsize=(15, 6))
sns.boxplot(data =fileSemanal, x= "nSemana", y= "Consumo", palette = sns.color_palette("Blues", 53))
plt.ylabel(f"Consumo ({unidade_consumo})")
save_fig("BoxplotSemanal")
plt.show()

# Calcular a autocorrelação para as 52 semanas
acf_values = sm.tsa.acf(fileSemanal['Consumo'], nlags=52) 

# Imprime o gráfico de autocorrelação
plt.figure(figsize=(10, 5))
plt.stem(range(len(acf_values)), acf_values)
plt.xlabel('semana')
plt.ylabel('Autocorrelação')
save_fig("AutocorrelacaoSemanal")
plt.show()

print("\n\n########### variáveis externas ###########\n")

# 1. Obter início e fim da série
start_date = fileDiario['Data'].min().strftime('%Y-%m-%d')
end_date = fileDiario['Data'].max().strftime('%Y-%m-%d')

# 2. Verificar o intervalo entre datas consecutivas
intervalo = fileDiario['Data'].sort_values().diff().mode()[0]
print(f'Intervalo de tempo da série: {intervalo}')

latitude = 39.7436
longitude = -8.8077

url = (f"https://archive-api.open-meteo.com/v1/archive?"
        f"latitude={latitude}&longitude={longitude}"
        f"&start_date={start_date}&end_date={end_date}"
        f"&daily=temperature_2m_max,temperature_2m_min,precipitation_sum"
        f"&timezone=Europe%2FLisbon"
)

res = requests.get(url)
data = res.json()

df_meteo = pd.DataFrame(data['daily'])
df_meteo['time'] = pd.to_datetime(df_meteo['time'])
df_meteo.rename(columns={'time': 'Data'}, inplace=True)

# 4. Merge com fileCompleto
fileDiario = fileDiario.merge(df_meteo, on='Data', how='left')

# Renomear colunas meteorológicas
fileDiario.rename(columns={
    'temperature_2m_max': 'temp_max',
    'temperature_2m_min': 'temp_min',
    'precipitation_sum': 'preci_sum'
}, inplace=True)

# Exibir o DataFrame resultante
print(fileDiario.head())

# Gráfico das variáveis meteorológicas
plt.figure(figsize=(15, 10))

# Subplot 1: Temperaturas
plt.subplot(3, 1, 1)
plt.plot(fileDiario['Data'], fileDiario['temp_max'], label='Temperatura Máxima', color='red', alpha=0.7)
plt.plot(fileDiario['Data'], fileDiario['temp_min'], label='Temperatura Mínima', color='blue', alpha=0.7)
plt.ylabel('Temperatura (°C)')
plt.legend()
plt.grid(True, alpha=0.3)

# Subplot 2: Precipitação
plt.subplot(3, 1, 2)
plt.bar(fileDiario['Data'], fileDiario['preci_sum'], color='steelblue', alpha=0.7, width=1)
plt.ylabel('Precipitação (mm)')
plt.grid(True, alpha=0.3)

# Subplot 3: Consumo
plt.subplot(3, 1, 3)
plt.plot(fileDiario['Data'], fileDiario['Consumo'], color='green', alpha=0.7)
plt.ylabel(f'Consumo ({unidade_consumo})')
plt.xlabel('Data')
plt.grid(True, alpha=0.3)

plt.tight_layout()
save_fig("VariaveisMeteorologicas")
plt.show()

# Selecionar apenas as colunas relevantes
variaveis = ['Consumo', 'temp_max', 'temp_min', 'preci_sum']
# Calcular correlação
correlacoes = fileDiario[variaveis].corr()

# Correlação entre as variáveis 
plt.figure(figsize=(6, 4))
sns.heatmap(correlacoes, annot=True, cmap='coolwarm', fmt='.2f')
plt.tight_layout()
save_fig("CorrelacaoVariaveisExternas")
plt.show()
