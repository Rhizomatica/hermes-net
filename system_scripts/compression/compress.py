#!/usr/bin/env python3
import torch, torchaudio, pickle, zlib, os, sys
from snac import SNAC

def compress(mp3_file):
    """Comprime MP3 para .snac"""
    model = SNAC.from_pretrained("hubertsiuzdak/snac_32khz").eval()
    audio, sr = torchaudio.load(mp3_file)
    if audio.shape[0] > 1: audio = audio.mean(dim=0, keepdim=True)
    if sr != 32000: audio = torchaudio.functional.resample(audio, sr, 32000); sr = 32000
    
    with torch.no_grad():
        codes = model.encode(audio.unsqueeze(0))
    
    data = {
        'codes': [c.cpu().numpy().astype('uint16') for c in codes],
        'sr': sr
    }
    
    snac_file = os.path.splitext(mp3_file)[0] + '.snac'
    with open(snac_file, 'wb') as f:
        f.write(zlib.compress(pickle.dumps(data)))
    
    print(f"Comprimido: {mp3_file} → {snac_file} ({os.path.getsize(snac_file)/1024:.1f}KB)")

# Uso automático
if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Uso: python3 snac_tiny.py arquivo.mp3  (comprime)")
        sys.exit(1)
    
    file = sys.argv[1]
    if file.endswith('.mp3'):
        compress(file)
    else:
        print("Arquivo deve ser .mp3 ou .snac")