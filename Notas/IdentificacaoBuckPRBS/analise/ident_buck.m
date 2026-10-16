% ============================================================================
%  ident_buck.m - identificacao ARX do conversor buck com PRBS (nota de identificacao do conversor buck)
%
%  Notacao (grande sinal = maiuscula, variacao = minuscula):
%      D  = D0 + d      razao ciclica,      D0  = 1/3
%      Vo = Vo0 + y     tensao de saida,    Vo0 = 10 V
%  G(s) = Y(s)/D(s) vale para as variacoes d -> y (modelo medio).
%
%  1. Modelo nominal e escolha de Ts (resposta ao degrau amostrada)
%  2. PRBS: Nb amostras por bit (Tb = Nb Ts), espectro e varredura de Nb
%  3. Ensaio simulado, identificacao ARX(2,2,2) por minimos quadrados
%  4. Validacao: PRBS nova (modelo x sistema conhecido) e onda quadrada do BK01
%  5. Efeito do nivel de ruido de medida
%
%  Ensaio simulado como na placa: modelo discreto exato (ZOH) + atraso de uma
%  amostra do firmware (o novo CMPA so vale no periodo seguinte) + ruido de medida.
%  Figuras em ../imgs/.
% ============================================================================
clear; clc; close all;

pasta = fileparts(mfilename('fullpath'));
imgs  = fullfile(pasta, '..', 'imgs');
if ~exist(imgs, 'dir'), mkdir(imgs); end

AZUL = [31 58 95]/255;  VERMELHO = [192 57 43]/255;
VERDE = [46 139 87]/255; CINZA = 0.35*[1 1 1];
set(groot, 'defaultAxesFontSize', 16, 'defaultLineLineWidth', 2, ...
    'defaultAxesBox', 'off', 'defaultAxesXGrid', 'on', 'defaultAxesYGrid', 'on');

%% 1. Modelo nominal
Gs  = tf(1.532e9, [1 2400 5.106e7]);

K    = dcgain(Gs);
[wn, zeta] = damp(Gs);  wn = wn(1);  zeta = zeta(1);
wd   = wn*sqrt(1 - zeta^2);
Td   = 2*pi/wd;                         % periodo da oscilacao amortecida
info = stepinfo(Gs);

fprintf('--- Modelo nominal ---\n');
fprintf('K = %.2f V   wn = %.0f rad/s (fn = %.0f Hz)   zeta = %.3f\n', K, wn, wn/2/pi, zeta);
fprintf('Td = %.3f ms   tr = %.3f ms   Mp = %.1f %%   ts = %.2f ms\n', ...
        1e3*Td, 1e3*info.RiseTime, info.Overshoot, 1e3*info.SettlingTime);

%% 1b. Periodo de amostragem
TsCand = [10 25 50 100 200]*1e-6;
fprintf('\n--- Candidatos a Ts ---\n');
fprintf('  Ts [us]  wn*Ts   amostras/Td  |z|     angulo [graus]\n');
for Tc = TsCand
    pz = exp(pole(Gs)*Tc);
    fprintf('  %6.0f   %5.2f   %8.1f    %.4f  %6.1f\n', 1e6*Tc, wn*Tc, Td/Tc, ...
            abs(pz(1)), rad2deg(abs(angle(pz(1)))));
end

% Resposta ao degrau D: 1/3 -> 0,4 (Vo: 10 -> 12 V) vista com dois Ts
D0 = 1/3;  Vo0 = 10;  dD = 0.4 - D0;
tc = linspace(0, 4e-3, 2000)';
yc = Vo0 + dD*step(Gs, tc);
figure('Position', [100 100 1400 480]);
for i = 1:2
    Tc = [50e-6 200e-6];  Tc = Tc(i);
    tk = (0:Tc:4e-3)';
    subplot(1,2,i);
    plot(1e3*tc, yc, 'Color', CINZA); hold on;
    stem(1e3*tk, Vo0 + dD*step(Gs, tk), 'filled', 'Color', VERMELHO, ...
         'BaseValue', Vo0, 'MarkerSize', 5);
    ylim([9.5 13.5]); xlabel('t [ms]'); ylabel('V_o [V]');
    title(sprintf('T_s = %.0f \\mus: %.1f amostras por oscilacao', 1e6*Tc, Td/Tc));
end
exportgraphics(gcf, fullfile(imgs, 'amostragem.png'), 'Resolution', 120);

Ts  = 50e-6;                            % = 1/fPWM: uma amostra por periodo do PWM
Gz  = c2d(Gs, Ts, 'zoh');               % modelo discreto exato (entrada em degraus)
Gzp = Gz * tf(1, [1 0], Ts);            % + atraso de uma amostra do firmware
[b0, a0] = tfdata(Gz, 'v');
fprintf('\nZOH (Ts = 50 us): b = [%.5f %.5f], a = [1 %.5f %.5f]\n', b0(2), b0(3), a0(2), a0(3));

%% 2. PRBS: bits no tempo (Nb = 1 e Nb = 4)
figure('Position', [100 100 1400 480]);
NbF = [1 4];
for i = 1:2
    q  = PRBS(40 + 30*NbF(i), NbF(i));      % mesmos bits nos dois casos:
    q  = q(30*NbF(i) + (1:40));             % a partir do bit 30, 40 amostras
    tk = (0:39)'*Ts;
    subplot(2,1,i);
    stairs(1e3*[tk; 40*Ts], [q; q(end)], 'Color', AZUL); hold on;
    plot(1e3*tk, q, 'o', 'Color', VERMELHO, 'MarkerFaceColor', VERMELHO, 'MarkerSize', 5);
    ylim([-1.4 1.4]); yticks([-1 1]); xlim([0 2]);
    ylabel('q');
    title(sprintf('N_b = %d: T_b = %.0f \\mus', NbF(i), 1e6*NbF(i)*Ts));
end
xlabel('t [ms]  (pontos = instantes de amostragem, T_s = 50 \mus)');
exportgraphics(gcf, fullfile(imgs, 'bits_prbs.png'), 'Resolution', 120);

%% 2b. Espectro da PRBS para varios Nb
N   = 2044;                             % um periodo da PRBS de 9 bits com Nb = 4
Ad  = 1/30;                             % amplitude: d = +-1/30 (Vo entre 9 e 11 V)
sig = 0.02;                             % ruido de medida [V] (ADC + ondulacao)
NbV = [1 2 4 8 16];

figure('Position', [100 100 1400 520]);
f = logspace(1, log10(0.5/Ts), 400);
[mg, ~] = bode(Gs, 2*pi*f);  mg = squeeze(mg);
semilogx(f, 20*log10(mg/K), 'k', 'LineWidth', 3); hold on;
cores = [AZUL; VERDE; VERMELHO; CINZA];
for i = 1:4
    x = pi*f*NbV(i)*Ts;
    semilogx(f, 10*log10((sin(x)./x).^2), 'Color', cores(i,:));   % forma sinc^2
end
xline(wn/2/pi, ':', 'f_n', 'FontSize', 15, 'LineWidth', 1.5);
ylim([-40 12]); xlim([f(1) f(end)]);
xlabel('f [Hz]'); ylabel('dB');
legend({'|G|/K', 'PRBS N_b = 1', 'N_b = 2', 'N_b = 4', 'N_b = 8'}, 'Location', 'southwest');
title('Espectro da PRBS (T_b = N_b T_s) e resposta em frequencia do buck');
exportgraphics(gcf, fullfile(imgs, 'espectro_prbs.png'), 'Resolution', 120);

%% 2c. Varredura de Nb (Monte Carlo: M realizacoes do ruido)
qv  = QuadradaBK01(N, Ts);              % validacao: onda quadrada do BK01
dv  = dD*qv;                            % d: 0 -> 1/15 (Vo: 10 <-> 12 V)
yv0 = lsim(Gzp, dv);                    % saida sem ruido (referencia)

M = 100;
fprintf('\n--- Varredura de Nb (M = %d, sigma = %.0f mV) ---\n', M, 1e3*sig);
fprintf('  Nb   Tb [us]  pulso max [ms]  fit val [%%]  erro K [%%]  erro wn [%%]  erro zeta [%%]\n');
rng(1);
for i = 1:numel(NbV)
    d  = Ad*PRBS(N, NbV(i));
    y0 = lsim(Gzp, d);
    r  = MonteCarlo(d, y0, sig, M, Ts, dv, yv0, K, wn, zeta);
    fprintf('  %2d   %5.0f     %6.2f        %6.1f      %6.2f     %6.2f      %6.2f\n', ...
            NbV(i), 1e6*NbV(i)*Ts, 1e3*9*NbV(i)*Ts, r);
end

%% 3. Ensaio simulado com o Nb escolhido
Nb = 4;
rng(2);
d  = Ad*PRBS(N, Nb);
y  = lsim(Gzp, d) + sig*randn(N,1);
t  = (0:N-1)'*Ts;

figure('Position', [100 100 1400 600]);
subplot(2,1,1); stairs(1e3*t, D0 + d, 'Color', AZUL); ylabel('D');
title(sprintf('Ensaio PRBS: T_s = 50 \\mus, N_b = %d (T_b = %.0f \\mus), %d amostras', Nb, 1e6*Nb*Ts, N));
xlim([0 40]); ylim([0.29 0.38]);
subplot(2,1,2); plot(1e3*t, Vo0 + y, 'Color', VERMELHO); ylabel('V_o [V]'); xlabel('t [ms]');
xlim([0 40]);
exportgraphics(gcf, fullfile(imgs, 'ensaio_prbs.png'), 'Resolution', 120);

%% 3b. Identificacao ARX(2,2,2) por minimos quadrados
[b, a] = ARX(d, y);
fprintf('\n--- ARX identificado (Nb = %d) ---\n', Nb);
fprintf('b = [%.5f %.5f]   a = [1 %.5f %.5f]\n', b(1), b(2), a(2), a(3));
Gid  = tf([0 0 b], [a 0], Ts);          % y(k) = ... + b1 d(k-2) + b2 d(k-3)
Gidc = tf(d2c(tf([0 b], a, Ts), 'zoh')); % sem o atraso do firmware -> G(s)
[nc, dc] = tfdata(Gidc, 'v');
[wi, zi] = damp(Gidc);
fprintf('G(s) identificada: [%.4g %.4g] / (s^2 + %.4g s + %.4g)\n', nc(end-1), nc(end), dc(2), dc(3));
fprintf('K = %.2f   wn = %.0f   zeta = %.3f\n', dcgain(Gidc), wi(1), zi(1));

%% 4. Validacao 1: PRBS nova (outro estado inicial), modelo x sistema conhecido
dn  = Ad*PRBS(N, Nb, [1 0 1 1 0 0 1 0 1]);
yn0 = lsim(Gzp, dn);                    % sistema conhecido (sem ruido)
ynm = lsim(Gid, dn);                    % simulacao livre do modelo ARX
fitn = 100*(1 - norm(yn0 - ynm)/norm(yn0 - mean(yn0)));
fprintf('Validacao (PRBS nova): fit = %.1f %%\n', fitn);

figure('Position', [100 100 1400 600]);
subplot(2,1,1); stairs(1e3*t, D0 + dn, 'Color', AZUL); ylabel('D');
title('Validacao: outro trecho da PRBS (outro estado inicial do registrador)');
xlim([0 20]); ylim([0.29 0.38]);
subplot(2,1,2);
plot(1e3*t, Vo0 + yn0, 'k', 'LineWidth', 3); hold on;
plot(1e3*t, Vo0 + ynm, '--', 'Color', VERMELHO, 'LineWidth', 2.4);
ylabel('V_o [V]'); xlabel('t [ms]'); xlim([0 20]); ylim([5.5 15]);
legend({'sistema conhecido G(s)', 'modelo ARX'}, 'Location', 'northeast', 'Orientation', 'horizontal');
title(sprintf('Simulacao livre do modelo: ajuste = %.1f %%', fitn));
exportgraphics(gcf, fullfile(imgs, 'validacao_prbs.png'), 'Resolution', 120);

%% 4b. Validacao 2: onda quadrada do BK01 (10 <-> 12 V)
rng(3);
yv  = yv0 + sig*randn(N,1);
yvm = lsim(Gid, dv);                    % simulacao livre do modelo
fitv = 100*(1 - norm(yv0 - yvm)/norm(yv0 - mean(yv0)));
fprintf('Validacao (degrau 10 <-> 12 V): fit = %.1f %%\n', fitv);

figure('Position', [100 100 1400 520]);
subplot(1,2,1);
plot(1e3*t, Vo0 + yv, 'Color', [CINZA 0.5]); hold on;
plot(1e3*t, Vo0 + yv0, 'k', 'LineWidth', 2.4);
plot(1e3*t, Vo0 + yvm, '--', 'Color', VERMELHO, 'LineWidth', 2.4);
xlim([0 40]); xlabel('t [ms]'); ylabel('V_o [V]');
legend({'medido (com ruido)', 'sistema conhecido', 'modelo ARX'}, 'Location', 'southeast');
title(sprintf('Onda quadrada 10 \\leftrightarrow 12 V (ajuste = %.1f %%)', fitv));
subplot(1,2,2);
w = 2*pi*logspace(1, log10(0.5/Ts), 300);
[m1, ~] = bode(Gz, w);  [m2, ~] = bode(tf([0 b], a, Ts), w);
semilogx(w/2/pi, 20*log10(squeeze(m1)), 'k-', 'LineWidth', 3); hold on;
semilogx(w/2/pi, 20*log10(squeeze(m2)), '--', 'Color', VERMELHO);
xlim([10 0.5/Ts]); ylabel('|G| [dB]'); xlabel('f [Hz]');
legend({'sistema conhecido (ZOH)', 'ARX'}, 'Location', 'southwest');
title('Modulo da resposta em frequencia');
exportgraphics(gcf, fullfile(imgs, 'validacao.png'), 'Resolution', 120);

%% 5. Efeito do nivel de ruido (Nb = 4)
fprintf('\n--- Efeito do ruido (Nb = 4, M = %d) ---\n', M);
fprintf('  sigma [mV]  fit val [%%]  erro K [%%]  erro wn [%%]  erro zeta [%%]\n');
rng(4);
d4 = Ad*PRBS(N, 4);  y4 = lsim(Gzp, d4);
for s = [0.005 0.01 0.02 0.05]
    r = MonteCarlo(d4, y4, s, M, Ts, dv, yv0, K, wn, zeta);
    fprintf('  %6.0f       %6.1f      %6.2f     %6.2f      %6.2f\n', 1e3*s, r);
end

%% ---------------------------------------------------------------------------
%  Funcoes
%  ---------------------------------------------------------------------------

% PRBS de +-1 com registrador de 9 bits (x^9 + x^5 + 1, periodo 511 bits);
% um bit novo a cada Nb amostras; reg0 = estado inicial (padrao: tudo 1)
function q = PRBS(N, Nb, reg0)
    if nargin < 3, reg0 = ones(1, 9); end
    reg = reg0;
    q   = zeros(N, 1);
    for k = 1:N
        if mod(k-1, Nb) == 0
            reg = [xor(reg(9), reg(5)), reg(1:8)];
        end
        q(k) = 2*reg(9) - 1;
    end
end

% Onda quadrada do BK01Degrau (0 = nivel baixo, 1 = nivel alto):
% 5 ms no nivel baixo, depois alterna a cada 10 ms
function q = QuadradaBK01(N, Ts)
    k  = (0:N-1)';
    k1 = round(5e-3/Ts);  nMeio = round(10e-3/Ts);
    q  = double(k >= k1 & mod(floor((k - k1)/nMeio), 2) == 0);
end

% ARX(2,2,2) por minimos quadrados (d e y: variacoes em torno do ponto de operacao)
%   y(k) = -a1 y(k-1) - a2 y(k-2) + b1 d(k-2) + b2 d(k-3)
function [b, a] = ARX(d, y)
    d = d - mean(d);  y = y - mean(y);
    k = (4:numel(y))';
    Phi   = [-y(k-1), -y(k-2), d(k-2), d(k-3)];
    theta = Phi \ y(k);
    a = [1, theta(1:2)'];
    b = theta(3:4)';
end

% M realizacoes do ruido: media do ajuste na validacao (contra a saida sem
% ruido) e dos erros relativos de K, wn e zeta do modelo continuo equivalente
function r = MonteCarlo(d, y0, sig, M, Ts, dv, yv0, K, wn, zeta)
    v = zeros(M, 4);
    for m = 1:M
        [b, a] = ARX(d, y0 + sig*randn(size(y0)));
        ym = lsim(tf([0 0 b], [a 0], Ts), dv);
        Gc = d2c(tf([0 b], a, Ts), 'zoh');
        [wi, zi] = damp(Gc);
        v(m,:) = [100*(1 - norm(yv0 - ym)/norm(yv0 - mean(yv0))), ...
                  abs(100*(dcgain(Gc)/K - 1)), abs(100*(wi(1)/wn - 1)), ...
                  abs(100*(zi(1)/zeta - 1))];
    end
    r = mean(v, 1);
end
