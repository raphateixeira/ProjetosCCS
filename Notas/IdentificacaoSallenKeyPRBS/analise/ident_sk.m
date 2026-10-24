% ============================================================================
%  ident_sk.m - identificacao ARX do filtro Sallen-Key (nota de identificacao)
%
%  Dados: ensaios SK02PRBS (PRBS 1,0 +- 0,4 V, Tb = 5 ms) e SK01Degrau (onda
%  quadrada 0,6 <-> 1,4 V), ambos gravados com Ts = 1 ms (dados/*SK01.csv).
%
%  1. ARX(2,2,1) com Ts = 1 ms x Ts = 5 ms (dados decimados) x OE(2,2,1)
%  2. Por que Ts = 1 ms falha: w*Ts e |z| dos polos
%  3. Escolha de Tb (Nb): espectro da PRBS e varredura simulada de Ts x Tb
%  4. Validacao: 30 % finais da PRBS e ensaio ao degrau
%
%  Figuras em ../imgs/ (prefixo sk_); os numeros dos slides vem da saida deste script.
% ============================================================================
clear; clc; close all;

pasta = fileparts(mfilename('fullpath'));
dados = fullfile(pasta, '..', 'dados');
imgs  = fullfile(pasta, '..', 'imgs');

AZUL = [31 58 95]/255;  VERMELHO = [192 57 43]/255;
VERDE = [46 139 87]/255; CINZA = 0.35*[1 1 1];
set(groot, 'defaultAxesFontSize', 16, 'defaultLineLineWidth', 2, ...
    'defaultAxesBox', 'off', 'defaultAxesXGrid', 'on', 'defaultAxesYGrid', 'on');

%% Dados (CSV do CCS: uV nas colunas 1-2, yV nas colunas 4-5)
Ts1  = 1e-3;                                   % Ts dos ensaios [s]
Ts   = 5e-3;                                   % Ts da identificacao [s]
q    = round(Ts/Ts1);
nPre = 100;                                    % 0,1 s em u0 antes da PRBS

M  = readmatrix(fullfile(dados, 'DadosPRBSSK01.csv'), 'NumHeaderLines', 2);
u1 = M(:,2);  y1 = M(:,5);
u0 = mean(u1(1:nPre));  y0 = mean(y1(1:nPre));
fprintf('Ruido de medida (u0 constante): %.2f mV\n', 1e3*std(y1(1:nPre)));

D  = readmatrix(fullfile(dados, 'DadosDegrauSK01.csv'), 'NumHeaderLines', 2);
ud1 = D(:,2);  yd1 = D(:,5);
nB  = find(abs(diff(ud1)) > 1e-6, 1);
yb  = mean(yd1(1:nB));

%% 1. ARX(2,2,1) com Ts = 1 ms e com Ts = 5 ms (decimado), e OE(2,2,1)
du1 = u1(nPre+1:end) - u0;   dy1 = y1(nPre+1:end) - y0;
du5 = du1(1:q:end);          dy5 = dy1(1:q:end);
n1 = round(0.7*numel(dy1));  n5 = round(0.7*numel(dy5));

[G1, th1] = ARX221(du1(1:n1), dy1(1:n1), Ts1);
[G5, th5] = ARX221(du5(1:n5), dy5(1:n5), Ts);
mOE = oe(iddata(dy5(1:n5), du5(1:n5), Ts), [2 2 1]);
Goe = tf(mOE.B, mOE.F, Ts, 'Variable', 'z^-1');
[~, ~, ~, thoe] = deal(0, 0, 0, [-mOE.F(2:3), mOE.B(2:3)]);

fprintf('\n--- Modelos ---\n');
fprintf('ARX 1 ms: a1 = %.4f a2 = %.4f b1 = %.5f b2 = %.5f\n', th1);
fprintf('ARX 5 ms: a1 = %.4f a2 = %.4f b1 = %.5f b2 = %.5f\n', th5);
fprintf('OE  5 ms: a1 = %.4f a2 = %.4f b1 = %.5f b2 = %.5f\n', thoe);
Resumo('ARX 1 ms', G1, Ts1);  Resumo('ARX 5 ms', G5, Ts);  Resumo('OE  5 ms', Goe, Ts);
Gc = tf(d2c(G5, 'zoh'));  [nc, dc] = tfdata(Gc, 'v');
fprintf('ARX 5 ms continuo: [%.4g %.4g] / (s^2 + %.4g s + %.4g)\n', nc(2), nc(3), dc(2), dc(3));

% Validacao no degrau
td1 = (0:numel(yd1)-1)'*Ts1;
ud5 = ud1(1:q:end);  yd5 = yd1(1:q:end);  td5 = (0:numel(yd5)-1)'*Ts;
yS1  = lsim(G1,  ud1 - ud1(1), td1) + yb;
yS5  = lsim(G5,  ud5 - ud5(1), td5) + yb;
ySoe = lsim(Goe, ud5 - ud5(1), td5) + yb;
fprintf('FIT degrau: ARX 1 ms = %.1f %%  ARX 5 ms = %.1f %%  OE = %.1f %%\n', ...
        Fit(yd1, yS1), Fit(yd5, yS5), Fit(yd5, ySoe));

% Validacao na PRBS (30 % finais)
t5 = (0:numel(dy5)-1)'*Ts;  t1 = (0:numel(dy1)-1)'*Ts1;
yP5  = lsim(G5, du5, t5);   yPoe = lsim(Goe, du5, t5);   yP1 = lsim(G1, du1, t1);
v5 = n5+1:numel(dy5);  v1 = n1+1:numel(dy1);
fprintf('FIT PRBS (30 %% finais): ARX 1 ms = %.1f %%  ARX 5 ms = %.1f %%  OE = %.1f %%\n', ...
        Fit(dy1(v1), yP1(v1)), Fit(dy5(v5), yP5(v5)), Fit(dy5(v5), yPoe(v5)));

figure('Position', [100 100 1400 520]);
plot(td1, yd1, 'Color', [CINZA 0.5], 'LineWidth', 4); hold on;
plot(td1, yS1, '-', 'Color', VERMELHO);
plot(td5, yS5, '--', 'Color', AZUL, 'LineWidth', 2.4);
xlim([0 1]); ylim([0.5 1.6]); xlabel('t [s]'); ylabel('y [V]');
legend({'medido', sprintf('ARX, T_s = 1 ms (ajuste %.0f %%)', Fit(yd1, yS1)), ...
        sprintf('ARX, T_s = 5 ms (ajuste %.0f %%)', Fit(yd5, yS5))}, 'Location', 'northwest');
title('Validação no ensaio ao degrau: simulação livre dos modelos');
exportgraphics(gcf, fullfile(imgs, 'sk_validacao_degrau.png'), 'Resolution', 120);

% Detalhe da subida: salto e patamar (so na subida) e as amostras em 5 ms
figure('Position', [100 100 1400 480]);
z1 = td1 >= 0.19 & td1 <= 0.26;  z5 = td5 >= 0.19 & td5 <= 0.26;
plot(td1(z1), yd1(z1), '.-', 'Color', VERMELHO, 'MarkerSize', 14); hold on;
plot(td5(z5), yd5(z5), 'o', 'Color', AZUL, 'MarkerFaceColor', AZUL, 'MarkerSize', 9);
stairs(td1(z1), 0.6 + 0.25*(ud1(z1) - 0.6), 'k:', 'LineWidth', 1.5);
xlabel('t [s]'); ylabel('y [V]'); xlim([0.19 0.26]);
legend({'amostras com T_s = 1 ms', 'amostras com T_s = 5 ms', 'u (escala reduzida)'}, ...
       'Location', 'northwest');
title('Subida do degrau: salto e patamar que o modelo de 2ª ordem não tem');
exportgraphics(gcf, fullfile(imgs, 'sk_pico.png'), 'Resolution', 120);

%% 2. w*Ts e |z| dos polos (modelo OE como referencia)
pc = sort(abs(pole(d2c(Goe, 'zoh'))));         % rad/s: polo lento e rapido
fprintf('\nPolos (OE, continuo): %.1f e %.1f rad/s (f = %.1f e %.1f Hz)\n', pc, pc/2/pi);
fprintf('  Ts [ms]   w1*Ts   w2*Ts   |z1|    |z2|\n');
for Tc = [1 2 5 10]*1e-3
    fprintf('  %4.0f      %.3f   %.3f   %.4f  %.4f\n', 1e3*Tc, pc*Tc, exp(-pc*Tc));
end
S = stepinfo(d2c(Goe, 'zoh'));
fprintf('Subida %.0f ms, acomodacao (2 %%) %.0f ms, banda %.1f Hz\n', ...
        1e3*S.RiseTime, 1e3*S.SettlingTime, bandwidth(d2c(Goe, 'zoh'))/2/pi);

%% 3. Espectro da PRBS (Tb = 5 ms e 20 ms) x |G|
figure('Position', [100 100 1400 520]);
f = logspace(-1, log10(100), 400);
[mg, ~] = bode(d2c(Goe, 'zoh'), 2*pi*f);  mg = squeeze(mg);
semilogx(f, 20*log10(mg/dcgain(Goe)), 'k', 'LineWidth', 3); hold on;
for Tb = [5 20]*1e-3
    x = pi*f*Tb;
    semilogx(f, 10*log10((sin(x)./x).^2), 'Color', ifelse(Tb < 0.01, CINZA, VERMELHO));
end
xline(pc/2/pi, ':', {'polo lento', 'polo rápido'}, 'FontSize', 14, 'LineWidth', 1.5);
ylim([-40 5]); xlim([f(1) f(end)]); xlabel('f [Hz]'); ylabel('dB');
legend({'|G|/K (filtro)', 'PRBS, T_b = 5 ms (ensaio)', 'PRBS, T_b = 20 ms (recomendado)'}, ...
       'Location', 'southwest');
title('Espectro da PRBS e resposta em frequência do filtro');
exportgraphics(gcf, fullfile(imgs, 'sk_espectro.png'), 'Resolution', 120);

%% 3b. Varredura simulada Ts x Tb (planta = OE, ruido medido, M realizacoes)
rng(1);  M = 50;  sig = std(y1(1:nPre));
Gref = d2c(Goe, 'zoh');
fprintf('\n--- Varredura simulada (FIT do ARX no degrau, media de %d) ---\n', M);
fprintf('  Ts\\Tb   5 ms   10 ms   20 ms   40 ms\n');
for Tc = [1 2 5 10]*1e-3
    Gz = c2d(Gref, Tc, 'zoh');  N = 2000;  tv = (0:N-1)'*Tc;
    qd = double(mod(floor(tv/0.25), 2) == 1);  yv0 = lsim(Gz, 0.8*qd, tv);
    linha = sprintf('  %2.0f ms ', 1e3*Tc);
    for Tb = [5 10 20 40]*1e-3
        Nb = round(Tb/Tc);
        if Nb < 1 || abs(Nb*Tc - Tb) > 1e-9, linha = [linha '     - ']; continue; end
        dq = 0.4*PRBS8(N, Nb);  yq = lsim(Gz, dq, tv);  fm = zeros(M,1);
        for m = 1:M
            G = ARX221(dq, yq + sig*randn(N,1), Tc);
            fm(m) = Fit(yv0, lsim(G, 0.8*qd, tv));
        end
        linha = [linha sprintf('%6.1f ', mean(fm))];
    end
    disp(linha);
end

%% 4. Ensaio (PRBS) e validacao nos 30 % finais
figure('Position', [100 100 1400 600]);
tP = (0:numel(u1)-1)'*Ts1;
subplot(2,1,1); stairs(tP, u1, 'Color', AZUL, 'LineWidth', 1.4); ylabel('u [V]');
xlim([0 1]); ylim([0.5 1.5]);
title('Ensaio SK02PRBS: T_s = 1 ms, T_b = 5 ms (primeiro segundo)');
subplot(2,1,2); plot(tP, y1, 'Color', VERMELHO); ylabel('y [V]'); xlabel('t [s]');
xlim([0 1]);
exportgraphics(gcf, fullfile(imgs, 'sk_ensaio_prbs.png'), 'Resolution', 120);

figure('Position', [100 100 1400 520]);
plot(t5(v5), y0 + dy5(v5), 'Color', [CINZA 0.5], 'LineWidth', 4); hold on;
plot(t5(v5), y0 + yP5(v5), '--', 'Color', AZUL, 'LineWidth', 2.4);
xlim(t5(v5([1 end]))); xlabel('t [s]'); ylabel('y [V]');
legend({'medido', sprintf('ARX, T_s = 5 ms (ajuste %.0f %%)', Fit(dy5(v5), yP5(v5)))}, ...
       'Location', 'northeast', 'Orientation', 'horizontal');
title('Validação: 30 % finais da PRBS (dados não usados na identificação)');
exportgraphics(gcf, fullfile(imgs, 'sk_validacao_prbs.png'), 'Resolution', 120);

%% ---------------------------------------------------------------------------
function [G, th] = ARX221(du, dy, Ts)
    % dy(k) = a1 dy(k-1) + a2 dy(k-2) + b1 du(k-1) + b2 du(k-2)
    k   = (3:numel(dy))';
    Phi = [dy(k-1), dy(k-2), du(k-1), du(k-2)];
    th  = (Phi \ dy(k))';
    G   = tf([0 th(3) th(4)], [1 -th(1) -th(2)], Ts, 'Variable', 'z^-1');
end

function Resumo(nome, G, Ts)
    fprintf('%s: K = %.3f   tau = %s ms\n', nome, dcgain(G), ...
            mat2str(sort(1e3*real(-Ts ./ log(pole(G))), 'descend').', 3));
end

function f = Fit(y, ys)
    f = 100 * (1 - norm(y - ys) / norm(y - mean(y)));
end

function c = ifelse(cond, a, b)
    if cond, c = a; else, c = b; end
end

% PRBS de +-1 igual a do firmware: LFSR de 8 bits, x^8 + x^6 + x^5 + x^4 + 1
function q = PRBS8(N, Nb)
    l = 1;  q = zeros(N,1);  b = 1;
    for k = 1:N
        if mod(k-1, Nb) == 0
            r = bitand(bitxor(bitxor(l, bitshift(l,-2)), bitxor(bitshift(l,-3), bitshift(l,-4))), 1);
            l = bitor(bitshift(l,-1), bitshift(r,7));
            b = 2*bitand(l,1) - 1;
        end
        q(k) = b;
    end
end
