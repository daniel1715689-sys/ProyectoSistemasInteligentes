import serial
import numpy as np
from vosk import Model, KaldiRecognizer
import json
from difflib import get_close_matches

PORT = 'COM3'
BAUD = 2000000
SAMPLE_RATE = 16000
GAIN = 5

ser = serial.Serial(PORT, BAUD)
model = Model("vosk-model-small-es-0.42")

rec = KaldiRecognizer(model, SAMPLE_RATE)

COLORES = ["rojo", "verde", "azul", "amarillo"]

COLOR_CMD = {
    "verde": b'0',
    "rojo": b'1',
    "amarillo": b'2',
    "azul": b'3'
}

# Funcion para corregir palabras con errores
def corregir_color(palabra):
    match = get_close_matches(palabra, COLORES, n=1, cutoff=0.6)
    return match[0] if match else None

print("Microfono captando audio \n")

while True:
    data = ser.read(1024)

    # convertir a numpy
    audio_np = np.frombuffer(data, dtype=np.int16)

    # amplificar
    audio_np = audio_np * GAIN
    audio_np = np.clip(audio_np, -32768, 32767)

    # regresar a bytes
    processed = audio_np.astype(np.int16).tobytes()

    # enviar a vosk
    if rec.AcceptWaveform(processed):
        result = json.loads(rec.Result())
        texto = result.get("text", "")

        if texto:
            print("Texto final:", texto)

            colores_detectados = []  # IMPORTANTE: reset por frase

            palabras = texto.split()

            for i, palabra in enumerate(palabras):
                color_corregido = corregir_color(palabra)

                if color_corregido:
                    colores_detectados.append((i, color_corregido))

            # ordenar por orden en la frase
            colores_detectados.sort()

            # enviar en orden correcto
            for _, color in colores_detectados:
                print("Enviando color al esp32:", color)
                comando = COLOR_CMD[color]
                ser.write(comando)

            # salir del programa
            if "salir" in texto or "adiós" in texto:
                print("\nTerminando programa")
                break

    else:
        partial = json.loads(rec.PartialResult())
        texto = partial.get("partial", "")
        if texto:
            print(texto)