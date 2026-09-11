"""Run from repository root; see docs/TUBE-CALIBRATION.md."""
import sys,numpy as np
from scipy.interpolate import BSpline
from scipy.signal import lfilter
sys.path.insert(0, str(__import__('pathlib').Path(__file__).resolve().parents[1]));from compare_audio import read
z=np.load('local/parity/tube-wet.npz');u=np.linspace(0,1,1501);models=[]
for branch in [-1,1]:
 xx=[];dd=[];yy=[]
 for d,c in zip(z['drives']/1e6,z['coefficients']):
  if branch>0 and d<=.25:continue
  f=BSpline(z['knots'],c,3);xx.append(u);dd.append(np.full(len(u),d));yy.append(branch*f(branch*u))
 xx=np.concatenate(xx);dd=np.concatenate(dd);yy=np.concatenate(yy)
 m=np.polynomial.chebyshev.chebvander2d(xx*2-1,dd*2-1,[12,4]).reshape(len(xx),-1);power=3 if branch<0 else 1;m*=xx[:,None]**power
 c=np.linalg.lstsq(m,yy,rcond=None)[0].reshape(13,5)
 assert np.isfinite(c).all()
 models.append(c)
 residual=np.einsum('ij,j->i',m,c.ravel())-yy
 assert np.isfinite(residual).all()
 print(branch,'surface max',abs(residual).max(),flush=True)
np.savez('local/parity/tube-polynomial.npz',negative=models[0],positive=models[1],denominator=z['denominator'],A=z['A'],trim=z['trim'])
