"""Run from repository root; see docs/TUBE-CALIBRATION.md."""
import sys,numpy as np
from scipy.interpolate import BSpline
from scipy.sparse import vstack
from scipy.sparse.linalg import lsqr
from scipy.signal import lfilter
sys.path.insert(0, str(__import__('pathlib').Path(__file__).resolve().parents[1]));from compare_audio import read
pos=np.unique(np.r_[np.geomspace(1e-5,.1,30),np.linspace(.1,1,65)]);kn=np.r_[-pos[::-1],0,0,0,pos];t=np.r_[[-1.]*3,kn,[1.]*3];a=np.load('local/parity/tube0-coeff.npz')['a'];scale=(338989/269055)**3;A=.54285348;trim=.49165056986916567/.5;models={}
bzero=BSpline.design_matrix(np.array([0.]),t,3).toarray()[0]
def w(x,d):return np.where(x<0,max(0,1-d/.75)**2,min(1,max(0,1-(d-.25)/.75))**2)
for d in [125000,250000,375000,488095,625000,750000,875000,1000000]:
 mats=[];rhs=[]
 for v in [1,16,32,64,96,127]:
  _,x=read(f'local/parity/dry-kontakt6-repeat/n12-v{v}-g144000.wav');_,y=read(f'local/parity/tube-k6-d{d}-g12-o269055/n12-v{v}-g144000.wav');x=x[:14000]*8;y=y[:14000]*scale
  b=[];dry=[]
  for i in range(4):
   z=x[3-i:len(x)-i].ravel();weights=w(z,d/1e6);basis=BSpline.design_matrix(np.clip(z/trim,-1,1),t,3)
   from scipy.sparse import csr_matrix
   basis=basis-csr_matrix(np.broadcast_to(bzero,basis.shape));b.append(basis.multiply(((1-weights)*A*trim)[:,None]));dry.append(weights*A*z)
  mats.append(b[0]-b[1]-b[2]+b[3]);rhs.append((y[3:]+a[1]*y[2:-1]+a[2]*y[1:-2]+a[3]*y[:-3]).ravel()-(dry[0]-dry[1]-dry[2]+dry[3]))
 m=vstack(mats);target=np.concatenate(rhs);sol=lsqr(m,target,atol=1e-12,btol=1e-12,iter_lim=1500);c=sol[0];f=BSpline(t,c,3);c-=f(0);models[d]=c
 print(d,'fit',np.linalg.norm(m@c-target)/np.linalg.norm(target),'endpoints',f([-1,0,1]),flush=True)
np.savez('local/parity/tube-wet.npz',knots=t,coefficients=np.array(list(models.values())),drives=np.array(list(models)),denominator=a,A=A,trim=trim)
for d,c in models.items():
 f=BSpline(t,c,3)
 for v in [48,100]:
  _,x=read(f'local/parity/dry-kontakt6-repeat/n12-v{v}-g144000.wav');_,y=read(f'local/parity/tube-k6-d{d}-g12-o269055/n12-v{v}-g144000.wav');x*=8;ww=w(x,d/1e6);s=ww*A*x+(1-ww)*A*trim*f(np.clip(x/trim,-1,1));p=lfilter([1,-1,-1,1],a,s,axis=0)/scale;print('test',d,v,np.linalg.norm(p-y)/np.linalg.norm(y),abs(p-y).max(),flush=True)
