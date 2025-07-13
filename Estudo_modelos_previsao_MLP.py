import warnings
import numpy as np
import pandas as pd
from sklearn.neural_network import MLPRegressor
from sklearn.preprocessing import StandardScaler
from sklearn.metrics import r2_score, mean_absolute_percentage_error
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import os
import joblib
import uuid
from time import time
from datetime import datetime
import optuna
import logging

from datetime import date, timedelta
from dateutil.easter import easter

# Configurar logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

warnings.filterwarnings('ignore')

# Configurações
output_dir = 'Result_MLP_2'
n_trials = 2000  

# Carregar e preparar dados
df = pd.read_csv('Daily_df.csv', sep=';', decimal=',')
df['Date'] = pd.to_datetime(df['Date'], format='%d/%m/%Y')
df.columns = ['data', 'consumo']
df['dia_da_semana'] = 7 - df['data'].dt.weekday


# === FERIADOS DE LEIRIA ===
def feriados_leiria(ano):
    pascoa = easter(ano)
    feriados = [
        date(ano, 1, 1),
        date(ano, 4, 25),
        date(ano, 5, 1),
        date(ano, 6, 10),
        date(ano, 8, 15),
        date(ano, 10, 5),
        date(ano, 11, 1),
        date(ano, 12, 1),
        date(ano, 12, 8),
        date(ano, 12, 25),
        pascoa - timedelta(days=2),  
        pascoa,                      
        date(ano, 6, 8),            
        date(ano, 5, 22),           
    ]
    return sorted(feriados)

# === CRIAR COLUNA DE FERIADOS ===
anos = df['data'].dt.year.unique()
feriados = pd.to_datetime([f for ano in anos for f in feriados_leiria(ano)])
df['feriado'] = df['data'].isin(feriados).astype(int)

# === DEFINIR OCUPAÇÃO ===
df['ocupacao'] = 0

for ano in anos:
    # Férias
    df.loc[(df['data'] >= f'{ano}-12-23') & (df['data'] <= f'{ano+1}-01-05'), 'ocupacao'] = 1
    df.loc[(df['data'] >= f'{ano}-04-14') & (df['data'] <= f'{ano}-04-21'), 'ocupacao'] = 1
    df.loc[df['data'].dt.month == 8, 'ocupacao'] = 1

# Exames
df.loc[(df['data'].dt.month == 1) & (df['data'].dt.day <= 24), 'ocupacao'] = 2
df.loc[(df['data'].dt.month == 6) & (df['data'].dt.day <= 20), 'ocupacao'] = 2

# Aulas normais
df.loc[((df['data'].dt.month >= 2) & (df['data'].dt.month <= 5)) |
       ((df['data'].dt.month >= 9) & (df['data'].dt.month <= 12)), 'ocupacao'] = 3

# Início do ano letivo (aplicado depois para ter prioridade)
df.loc[(df['data'].dt.month == 9) & (df['data'].dt.day <= 10), 'ocupacao'] = 4

# Feriados sobrepõem tudo
df.loc[df['feriado'] == 1, 'ocupacao'] = 1



# Adicionar colunas de lag (até 14 dias)
for i in range(1, 22):
    df[f'consumo_{i}d_antes'] = df['consumo'].shift(i)
df.dropna(inplace=True)


# Validar dados
if df[['consumo'] + [f'consumo_{i}d_antes' for i in range(1, 22)]].isna().any().any():
    raise ValueError("Dados contêm valores NaN após preenchimento de lags.")
if not np.isfinite(df[['consumo'] + [f'consumo_{i}d_antes' for i in range(1, 22)]].values).all():
    raise ValueError("Dados contêm valores infinitos.")

# Função para treinar e avaliar MLP com Optuna
def train_and_evaluate(X_train, y_train, X_test, y_test, feature_set_name, trial_number, n_trials_total):
    global modelos_treinados
    global inicio
    global all_trials_df

    # Validar entradas
    if np.any(~np.isfinite(X_train.values)) or np.any(~np.isfinite(y_train.values)):
        logger.error(f"Trial {trial_number}: Dados de treino contêm NaN ou valores infinitos.")
        return None
    if np.any(~np.isfinite(X_test.values)) or np.any(~np.isfinite(y_test.values)):
        logger.error(f"Trial {trial_number}: Dados de teste contêm NaN ou valores infinitos.")
        return None

    # Normalizar entradas e saídas
    scaler_X = StandardScaler()
    X_train_scaled = scaler_X.fit_transform(X_train) if X_train.ndim > 1 else scaler_X.fit_transform(X_train.values.reshape(-1, 1))
    X_test_scaled = scaler_X.transform(X_test) if X_test.ndim > 1 else scaler_X.transform(X_test.values.reshape(-1, 1))
    
    scaler_y = StandardScaler()
    y_train_scaled = scaler_y.fit_transform(y_train.values.reshape(-1, 1)).ravel()

    # Criar diretório para modelos
    model_dir = os.path.join(output_dir, 'models')
    os.makedirs(model_dir, exist_ok=True)
    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')

    # Dicionário para armazenar caminhos dos modelos por trial
    trial_models = {}

    # Função objetivo para Optuna
    def objective(trial):
        # Definir espaço de busca de hiperparâmetros
        n_layers = trial.suggest_int('n_layers', 1, 4)
        hidden_layer_sizes = tuple(trial.suggest_int(f'layer_{i}', 10, 200) for i in range(n_layers))
        params = {
            'hidden_layer_sizes': hidden_layer_sizes,
            'activation': trial.suggest_categorical('activation', ['relu', 'tanh', 'logistic']),
            'solver': trial.suggest_categorical('solver', ['adam', 'sgd']),
            'alpha': trial.suggest_float('alpha', 1e-5, 1e-1, log=True),
            'learning_rate_init': trial.suggest_float('learning_rate_init', 1e-4, 1e-2, log=True),
            'batch_size': trial.suggest_categorical('batch_size', [16, 32, 64, 128]),
            'max_iter': 5000,
            'early_stopping': True,
            'validation_fraction': 0.15,
            'n_iter_no_change': trial.suggest_int('n_iter_no_change', 5, 20),
            'random_state': trial.suggest_int('random_state', 0, 1000)
        }

        # Criar e treinar o modelo
        model = MLPRegressor(**params)
        model_path = os.path.join(model_dir, f"MLP_{feature_set_name.replace(' ', '_')}_{trial_number}_trial_{trial.number}_{timestamp}_{uuid.uuid4()}.model")
        try:
            model.fit(X_train_scaled, y_train_scaled)
            y_pred_scaled = model.predict(X_test_scaled)
            # Desnormalizar previsões
            y_pred = scaler_y.inverse_transform(y_pred_scaled.reshape(-1, 1)).ravel()
            # Clipar previsões para evitar valores extremos
            y_pred = np.clip(y_pred, y_train.min(), y_train.max())
            # Verificar NaN nas previsões
            if not np.isfinite(y_pred).all():
                logger.warning(f"Trial {trial_number}, Optuna Trial {trial.number}: Previsões contêm NaN ou valores infinitos.")
                raise optuna.TrialPruned()
            mape = mean_absolute_percentage_error(y_test, y_pred) * 100
            r2 = r2_score(y_test, y_pred)
            rmse = np.sqrt(np.mean((y_test - y_pred) ** 2))

            # Salvar o modelo
            joblib.dump(model, model_path)
            trial_models[trial.number] = model_path
            # Armazenar resultado do trial
            trial_result = {
                'model_type': 'MLP',
                'feature_set': feature_set_name,
                'trial_number': trial_number,
                'optuna_trial': trial.number,
                'config': str(params),
                'mape_test': mape,
                'r2_test': r2,
                'rmse_test': rmse,
                'timestamp': datetime.now().strftime('%Y%m%d_%H%M%S'),
                'training_time': time() - inicio,
                'model_path': model_path
            }
            global all_trials_df
            all_trials_df = pd.concat([all_trials_df, pd.DataFrame([trial_result])], ignore_index=True)
            return r2  # Otimizar com r2
        except Exception as e:
            logger.error(f"Trial {trial_number}, Optuna Trial {trial.number}: Erro durante treinamento: {str(e)}")
            raise optuna.TrialPruned()

    # Callback para logar o melhor modelo após cada trial
    def log_best_trial(study, trial):
        if study.best_trial.number == trial.number:
            logger.info(f"Trial {trial_number}, Optuna Trial {trial.number}: Novo melhor MAPE: {study.best_value:.2f}, Parâmetros: {study.best_params}")

    # Criar estudo Optuna
    study = optuna.create_study(direction='maximize')
    try:
        study.optimize(objective, n_trials, show_progress_bar=True, callbacks=[log_best_trial])
    except Exception as e:
        logger.error(f"Trial {trial_number}: Falha no estudo Optuna: {str(e)}")
        return None

    # Verificar se o estudo encontrou parâmetros válidos
    if not study.best_params:
        logger.error(f"Trial {trial_number}: Nenhum parâmetro válido encontrado.")
        return None

    # Retornar o melhor modelo encontrado pelo Optuna apenas deste treino
    best_trial = study.best_trial
    best_model_path = trial_models.get(best_trial.number)
    # Filtrar apenas os trials deste treino atual (trial_number)
    mask = (all_trials_df['trial_number'] == trial_number) & (all_trials_df['optuna_trial'] == best_trial.number)
    trial_row = all_trials_df.loc[mask].iloc[0]
    trial_result = {
        'model_type': 'MLP',
        'feature_set': feature_set_name,
        'trial_number': trial_number,
        'optuna_trial': best_trial.number,
        'config': str(best_trial.params),
        'mape_test': trial_row['mape_test'],
        'r2_test': trial_row['r2_test'],
        'rmse_test': trial_row['rmse_test'],
        'timestamp': trial_row['timestamp'],
        'training_time': trial_row['training_time'],
        'model_path': best_model_path
    }
    return trial_result

# Dividir dados
data_corte1 = pd.to_datetime("2022-01-01")
data_corte2 = pd.to_datetime("2023-01-01")
train_df = df[df['data'] <= data_corte1]
valid_df = df[(df['data'] > data_corte1) & (df['data'] <= data_corte2)]
test_df = df[df['data'] > data_corte2]

# Definir conjuntos de features
feature_sets = [
    [f'consumo_{i}d_antes' for i in range(1, 8)],
    [f'consumo_{i}d_antes' for i in range(1, 15)],
    [f'consumo_{i}d_antes' for i in range(1, 22)],
    ['feriado'] + [f'consumo_{i}d_antes' for i in range(1, 15)],
    ['ocupacao'] + [f'consumo_{i}d_antes' for i in range(1, 15)],
    ['dia_da_semana'] + [f'consumo_{i}d_antes' for i in range(1, 15)],
    ['feriado', 'ocupacao', 'dia_da_semana'] + [f'consumo_{i}d_antes' for i in range(1, 15)],
]

# Configurações do experimento
n_trials_total = len(feature_sets)  # Um estudo por feature set
all_trials_df = pd.DataFrame(columns=['model_type', 'feature_set', 'trial_number', 'optuna_trial', 'config', 'mape_test', 'r2_test', 'rmse_test', 'timestamp', 'training_time', 'model_path'])
final_results = []
modelos_treinados = 0
inicio = time()

# Treinamento
for i, features in enumerate(feature_sets):
    logger.info(f"\nTreino para Feature {i+1}:")
    X_train = train_df[features]
    y_train = train_df['consumo']
    X_valid = valid_df[features]
    y_valid = valid_df['consumo']
    feature_set_name = f"Feature {i+1}"
    trial_number = i + 1
    result = train_and_evaluate(X_train, y_train, X_valid, y_valid, feature_set_name, trial_number, n_trials_total)
    if result:
        final_results.append(result)
        logger.info(f"Trial {trial_number}: Modelo={result['model_type']}, Config={result['config']}, "
                    f"MAPE: {result['mape_test']:.2f}%")
    else:
        logger.warning(f"Trial {trial_number}: Falhou, pulando para o próximo.")

# Exibir os melhores modelos
logger.info("\nMelhores modelos:")
for result in sorted(final_results, key=lambda x: x['mape_test']):
    logger.info(f"Modelo: {result['model_type']}, Feature: {result['feature_set']}, "
                f"Trial: {result['trial_number']}, MAPE: {result['mape_test']:.2f}%")

# Exportar resultados para Excel
os.makedirs(output_dir, exist_ok=True)
with pd.ExcelWriter(os.path.join(output_dir, 'results.xlsx'), engine='openpyxl') as writer:
    final_results_df = pd.DataFrame(final_results).drop(columns=['model_path'])
    final_results_df.sort_values(by='mape_test').to_excel(writer, sheet_name='Melhores Modelos', index=False)
    all_trials_df.drop(columns=['model_path']).to_excel(writer, sheet_name='All_Trials', index=False)

# Gerar gráficos para os melhores modelos
for idx, result in enumerate(sorted(final_results, key=lambda x: x['mape_test'])):
    feature_set_name = result['feature_set']
    trial_number = result['trial_number']
    model_path = result['model_path']
    features = feature_sets[int(feature_set_name.split()[-1]) - 1]
    X_train = train_df[features]
    y_train = train_df['consumo']
    X_test = test_df[features]
    y_test = test_df['consumo']
    dates = test_df['data'].values

    scaler_X = StandardScaler()
    X_train_scaled = scaler_X.fit_transform(X_train) if X_train.ndim > 1 else scaler_X.fit_transform(X_train.values.reshape(-1, 1))
    X_test_scaled = scaler_X.transform(X_test) if X_test.ndim > 1 else scaler_X.transform(X_test.values.reshape(-1, 1))
    scaler_y = StandardScaler()
    y_train_scaled = scaler_y.fit_transform(y_train.values.reshape(-1, 1)).ravel()

    # Carregar o melhor modelo salvo pelo Optuna
    try:
        model = joblib.load(model_path)
        y_pred_scaled = model.predict(X_test_scaled)
        y_pred = scaler_y.inverse_transform(y_pred_scaled.reshape(-1, 1)).ravel()
        y_pred = np.clip(y_pred, y_train.min(), y_train.max())
        y_true = y_test.values

        # Calcular métricas para o título
        r2 = r2_score(y_true, y_pred)
        mape = mean_absolute_percentage_error(y_true, y_pred) * 100
        rmse = np.sqrt(np.mean((y_true - y_pred) ** 2))

        # Gerar gráfico
        plt.style.use('seaborn')
        plt.figure(figsize=(12, 5))
        plt.plot(dates, y_true, label='Consumo Real', color='black')
        plt.plot(dates, y_pred, label='Consumo Previsto', color='red', alpha=0.7)
        plt.title(f"R²: {r2:.4f}, MAPE: {mape:.2f}%, RMSE: {rmse:.2f}")
        plt.xlabel('Data')
        plt.ylabel('Consumo')
        plt.legend()
        plt.xticks(rotation=45)
        plt.tight_layout()
        plt.savefig(os.path.join(output_dir, f"Pos_{idx+1}_{feature_set_name.replace(' ', '_')}_{trial_number}.png"))
        plt.close()
    except Exception as e:
        logger.error(f"Erro ao gerar gráfico para modelo {model_path}: {str(e)}")

logger.info(f"Resultados salvos em '{output_dir}/results.xlsx'.")