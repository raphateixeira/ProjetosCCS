%% Identificação ARX(2,2,1) do filtro Sallen-Key — ensaio PRBS (SK02PRBS)
%
% Modelo, em desvios em torno do ponto de operação:
%
%   y(k) = a1 y(k-1) + a2 y(k-2) + b1 u(k-1) + b2 u(k-2)
%
% Os parâmetros são estimados por mínimos quadrados com 70 % dos dados; os
% 30 % restantes servem para validar o modelo.
%
% Uso: abrir esta pasta (dados/) no MATLAB e rodar o script.

clear; clc; close all;

%% Dados do ensaio
Ts   = 1e-3;     % período de amostragem [s]
nPre = 100;      % amostras no ponto de operação antes da PRBS
uOp  = 1.0;      % ponto de operação da entrada [V]

% CSV do CCS: yV nas colunas 1-2 e uV nas colunas 4-5 (Sample, Value)
M = readmatrix('DadosPRBS1ms.csv', 'NumHeaderLines', 2);
y = M(:, 2);
u = M(:, 5);
N = length(y);
t = (0:N-1)' * Ts;

%% Desvios em torno do ponto de operação
yOp = mean(y(1:nPre));
du = u - uOp;
dy = y - yOp;

%% Identificação: primeiros 70 % das amostras
nId = round(0.7 * N);

% Cada linha de Phi é [y(k-1) y(k-2) u(k-1) u(k-2)]; o vetor Y tem y(k)
k   = (3:nId)';
Phi = [dy(k-1), dy(k-2), du(k-1), du(k-2)];
Y   = dy(k);

theta = Phi \ Y;          % mínimos quadrados
a1 = theta(1);  a2 = theta(2);
b1 = theta(3);  b2 = theta(4);

%% Modelo como função de transferência
%   G(z) = (b1 z^-1 + b2 z^-2) / (1 - a1 z^-1 - a2 z^-2)
G = tf([0 b1 b2], [1 -a1 -a2], Ts, 'Variable', 'z^-1')

polos = pole(G)
tau   = -Ts ./ log(polos)       % constantes de tempo [s]
K     = dcgain(G)               % ganho estático
Gc    = d2c(G, 'zoh')           % modelo contínuo equivalente

%% Validação: simulação livre do modelo com a entrada medida
ySim = lsim(G, du, t);

val = nId+1:N;                  % amostras de validação
erro = dy(val) - ySim(val);
FIT = 100 * (1 - norm(erro) / norm(dy(val) - mean(dy(val))))   % [%]

%% Gráficos
figure('Color', 'w');

subplot(2, 1, 1);
plot(t, u); grid on;
ylabel('u [V]');
title('Entrada: PRBS');

subplot(2, 1, 2);
plot(t, y, 'Color', [0.6 0.6 0.6]); hold on;
plot(t, ySim + yOp, 'r', 'LineWidth', 1.2);
xline(t(nId), '--k', 'início da validação');
grid on;
xlabel('t [s]'); ylabel('y [V]');
legend('medido', 'modelo ARX', 'Location', 'best');
title(['Saída: FIT na validação = ' num2str(FIT, '%.1f') ' %']);
