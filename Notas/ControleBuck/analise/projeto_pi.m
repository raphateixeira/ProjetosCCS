% ============================================================================
%  projeto_pi.m - projeto de um controlador PI digital para o conversor buck
%
%  Planta identificada (u -> y, Ts = 50 us):
%      G(z) = B(z)/A(z),  B = b1 z^-1 + b2 z^-2,  A = 1 + a1 z^-1 + a2 z^-2
%
%  PI digital (integrador de Euler para tras):
%      C(z) = Kp + Ki Ts z/(z - 1)
%      u(k) = u(k-1) + q0 e(k) + q1 e(k-1),   q0 = Kp + Ki Ts,  q1 = -Kp
%
%  Projeto: escolhe-se a frequencia de cruzamento fc e a margem de fase MF;
%  Kp e Ki saem de C(e^{jwcTs}) G(e^{jwcTs}) = 1 /_ (-180 + MF).
% ============================================================================
clear; clc; close all;

%% Planta
B  = [0, 0.01594, -0.03983];
A  = [1, -1.729, 0.7917];
Ts = 50e-6;

G = tf(B, A, Ts, 'Variable', 'z^-1');

fprintf('Ganho DC da planta: %.4f\n', dcgain(G));
fprintf('Polos: %s\n', mat2str(pole(G).', 4));
fprintf('Zeros: %s\n', mat2str(zero(G).', 4));
if dcgain(G) < 0
    fprintf('ATENCAO: ganho DC negativo (buck: u -> y deveria ter ganho ~ +1).\n');
end
if any(abs(zero(G)) > 1)
    fprintf('ATENCAO: zero fora do circulo unitario (fase nao minima).\n');
end

%% Especificacoes
fc = 500;          % frequencia de cruzamento [Hz] (abaixo da ressonancia LC, ~850 Hz)
MF = 70;           % margem de fase [graus]

%% Calculo de Kp e Ki
wc  = 2*pi*fc;
zc  = exp(1j*wc*Ts);                    % z na frequencia de cruzamento
Gc  = evalfr(G, zc);                    % resposta da planta em wc
Cd  = exp(1j*deg2rad(-180 + MF)) / Gc;  % C(zc) necessario
I   = Ts*zc/(zc - 1);                   % parte integral em zc

% Cd = Kp + Ki*I  ->  duas equacoes reais (parte real e imaginaria)
x  = [1, real(I); 0, imag(I)] \ [real(Cd); imag(Cd)];
Kp = x(1);
Ki = x(2);

fprintf('\nKp = %.5f\nKi = %.3f\n', Kp, Ki);
if Kp < 0 || Ki < 0
    fprintf('ATENCAO: ganho negativo - mude fc ou MF (ou reveja o modelo).\n');
end
fprintf('q0 = Kp + Ki*Ts = %.6f\nq1 = -Kp        = %.6f\n', Kp + Ki*Ts, -Kp);

C = Kp + Ki*Ts*tf([1 0], [1 -1], Ts);

%% Verificacao
L  = C*G;                    % malha aberta
T  = feedback(L, 1);         % malha fechada r -> y
S  = feedback(C, G);         % r -> u (esforco de controle)

fprintf('\nPolos de malha fechada (|z|): %s\n', mat2str(abs(pole(T)).', 4));
if all(abs(pole(T)) < 1)
    fprintf('Malha fechada estavel.\n');
else
    fprintf('ATENCAO: malha fechada instavel!\n');
end
[MG, MFr] = margin(L);
fprintf('Margem de ganho: %.1f dB   Margem de fase: %.1f graus\n', 20*log10(MG), MFr);

figure; margin(L); grid on;
title(sprintf('Malha aberta C G  (fc = %g Hz, MF = %g graus)', fc, MF));

figure;
subplot(2,1,1); step(T, 20e-3); grid on; ylabel('y [V]');
title('Degrau unitario na referencia');
subplot(2,1,2); step(S, 20e-3); grid on; ylabel('u [V]'); title('');
