#!/usr/bin/env python3
import torch, torchaudio, pickle, zlib, os, sys, subprocess
from snac import SNAC
import gc

def decompress(snac_file):
    """Descomprime .snac para WAV e MP3 usando CPU"""
    # Limpar memória
    torch.cuda.empty_cache() if torch.cuda.is_available() else None
    gc.collect()
    
    # Forçar CPU
    torch.set_num_threads(4)  # Ajuste conforme seu CPU
    
    model = SNAC.from_pretrained("hubertsiuzdak/snac_32khz").eval()
    
    with open(snac_file, 'rb') as f:
        data = pickle.loads(zlib.decompress(f.read()))
    
    codes = [torch.from_numpy(arr.astype('int64')) for arr in data['codes']]
    
    # Sempre usar CPU
    device = 'cpu'
    model = model.to(device)
    codes = [c.to(device) for c in codes]
    
    print(f"Decodificando {snac_file} na CPU...")
    
    with torch.no_grad():
        audio = model.decode(codes).squeeze(0)
    
    # Extrair nome base do arquivo (sem extensão e sem caminho)
    base_name = os.path.splitext(os.path.basename(snac_file))[0]
    
    # Criar nomes com sufixo Decompress
    wav_file = f"{base_name}Decompress.wav"
    mp3_file = f"{base_name}Decompress.mp3"
    
    # Se quiser salvar no mesmo diretório do arquivo original
    output_dir = os.path.dirname(snac_file) if os.path.dirname(snac_file) else "."
    
    # Caminhos completos
    wav_path = os.path.join(output_dir, wav_file)
    mp3_path = os.path.join(output_dir, mp3_file)
    
    # Salvar WAV
    torchaudio.save(wav_path, audio, data['sr'])
    print(f"✓ Salvo: {wav_path}")
    
    # Converter para MP3
    try:
        subprocess.run(['ffmpeg', '-i', wav_path, '-q:a', '2', '-y', mp3_path], 
                       capture_output=True, check=True)
        print(f"✓ Convertido: {mp3_path}")
        
        # Opcional: remover o arquivo WAV após criar MP3
        # os.remove(wav_path)
        # print(f"✓ Removido arquivo WAV temporário")
        
    except subprocess.CalledProcessError as e:
        print(f"✗ Erro ao converter para MP3: {e}")
        print("Tentando conversão alternativa...")
        
        # Tentar método alternativo se ffmpeg falhar
        try:
            # Usar torchaudio para salvar como mp3 (se suportado)
            torchaudio.save(mp3_path, audio, data['sr'], format='mp3')
            print(f"✓ MP3 salvo via torchaudio: {mp3_path}")
        except:
            print("✗ Não foi possível criar MP3. Mantendo apenas WAV.")
    
    print(f"\n✅ Descompressão completa!")
    print(f"   Arquivo original: {snac_file}")
    print(f"   WAV criado: {wav_path}")
    print(f"   MP3 criado: {mp3_path}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Uso: python3 decompress_cpu.py arquivo.snac")
        print("\nExemplos:")
        print("  python3 decompress_cpu.py musica.snac")
        print("  python3 decompress_cpu.py /caminho/para/audio.snac")
        sys.exit(1)
    
    file = sys.argv[1]
    
    if not file.endswith('.snac'):
        print("✗ Erro: Arquivo deve ter extensão .snac")
        sys.exit(1)
    
    if not os.path.exists(file):
        print(f"✗ Erro: Arquivo não encontrado: {file}")
        sys.exit(1)
    
    decompress(file)