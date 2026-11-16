"""Business core: no host or frontend imports."""
import hashlib

def analyze(text):
    return {'characters':len(text),'lines':len(text.splitlines()),'words':len(text.split()),'sha256':hashlib.sha256(text.encode('utf-8')).hexdigest()}
