import unittest
from route_grid import find_path


class RoutingTests(unittest.TestCase):
    def blank(self,n=9): return [bytearray(n*n),bytearray(n*n)],bytearray(n*n)

    def test_clear_route_stays_on_top(self):
        blocks,vias=self.blank()
        path=find_path(blocks,vias,9,9,(1,1),(7,7))
        self.assertEqual(path[0][:2],(1,1));self.assertEqual(path[-1][:2],(7,7))
        self.assertTrue(all(p[2]==0 for p in path))

    def test_wall_requires_layer_change(self):
        blocks,vias=self.blank()
        for y in range(9):
            blocks[0][y*9+4]=1
            blocks[1][y*9+6]=1
        path=find_path(blocks,vias,9,9,(1,4),(7,4))
        self.assertIsNotNone(path)
        self.assertTrue(any(a[2]!=b[2] for a,b in zip(path,path[1:])))

    def test_impossible_wall_returns_no_route(self):
        blocks,vias=self.blank()
        for z in range(2):
            for y in range(9): blocks[z][y*9+4]=1
        self.assertIsNone(find_path(blocks,vias,9,9,(1,4),(7,4)))

    def test_diagonal_cannot_cut_through_corner(self):
        blocks,vias=self.blank(2)
        for z in range(2): blocks[z][1]=blocks[z][2]=1
        self.assertIsNone(find_path(blocks,vias,2,2,(0,0),(1,1)))


if __name__=='__main__': unittest.main()
