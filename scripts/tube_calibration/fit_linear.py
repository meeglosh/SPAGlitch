"""Run from repository root; see docs/TUBE-CALIBRATION.md."""
import sys,numpy as np
from scipy.signal import lfilter
from scipy.optimize import least_squares
sys.path.insert(0, str(__import__('pathlib').Path(__file__).resolve().parents[1]));from compare_audio import read
_,x=read('local/parity/dry-kontakt6-repeat/n12-v127-g144000.wav');_,y=read('local/parity/tube-only-k6-drive0-outminus4p1/n12-v127-g144000.wav');x=x[:30000];y=y[:30000]
def coeff(p):
 r,t,lp,g=p
 return np.array([1,-1,-1,1])*g,np.convolve([1,-2*r*np.cos(t),r*r],[1,-lp])
def fun(p):
 b,a=coeff(p);return (lfilter(b,a,x,axis=0)-y).ravel()
r=least_squares(fun,[.996269,.003722,-.748475,.542854],bounds=([.9,0,-.999,.1],[.999999,.1,0,10]),xtol=1e-13,ftol=1e-13,gtol=1e-13,max_nfev=100)
print(r.x,np.linalg.norm(r.fun)/np.linalg.norm(y),abs(r.fun).max());b,a=coeff(r.x);np.savez('local/parity/tube0-coeff.npz',b=b,a=a,p=r.x)
