"""Small deterministic two-layer maze router; standard library only.

Geometry is conservatively rasterized by the KiCad adapter. KiCad DRC remains
the independent authority for actual copper clearances/connectivity.
"""
import heapq
import math


def find_path(blocked, via_blocked, width, height, start, goal, via_cost=80,
              bottom_cost=1.4):
    """Return (x,y,layer) cells; disallow diagonal corner cutting."""
    def key(x,y,z): return (z*height+y)*width+x
    def decode(k):
        x=k%width; q=k//width
        return x,q%height,q//height
    def heuristic(x,y):
        dx,dy=abs(goal[0]-x),abs(goal[1]-y)
        return max(dx,dy)+(math.sqrt(2)-1)*min(dx,dy)
    queue=[]; costs={}; previous={}
    for z in range(2):
        if not blocked[z][start[1]*width+start[0]]:
            k=key(*start,z);costs[k]=0
            heapq.heappush(queue,(heuristic(*start),0,k))
    steps=[(1,0),(-1,0),(0,1),(0,-1),(1,1),(1,-1),(-1,1),(-1,-1)]
    while queue:
        _,cost,k=heapq.heappop(queue)
        if cost!=costs.get(k): continue
        x,y,z=decode(k)
        if (x,y)==goal:
            path=[]
            while True:
                path.append(decode(k))
                if k not in previous: break
                k=previous[k]
            return path[::-1]
        candidates=[]
        for dx,dy in steps:
            xx,yy=x+dx,y+dy
            if not (0<=xx<width and 0<=yy<height): continue
            if blocked[z][yy*width+xx]: continue
            if dx and dy and (blocked[z][y*width+xx] or blocked[z][yy*width+x]): continue
            candidates.append((xx,yy,z,(math.sqrt(2) if dx and dy else 1)*(bottom_cost if z else 1)))
        if not via_blocked[y*width+x] and not blocked[1-z][y*width+x]:
            candidates.append((x,y,1-z,via_cost))
        for xx,yy,zz,delta in candidates:
            kk=key(xx,yy,zz);c=cost+delta
            if c<costs.get(kk,float('inf')):
                costs[kk]=c;previous[kk]=k
                heapq.heappush(queue,(c+heuristic(xx,yy),c,kk))
    return None
