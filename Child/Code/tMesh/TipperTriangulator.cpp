//A triangulation routine based on Tipper's convex hull algorithm
//Computers and geoscience vol17 no 5 pp 597-632,1991
//Scaling is nlogn for random datasets
//Mike Bithell 31/08/01
#include <math.h>
#include <fstream.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

#define DEBUG_PRINT 1

class point{
public:
  point() : x(0.), y(0.) {}
  point(double ix,double iy) : x(ix), y(iy) {}
  point(const point& p) : x(p.x), y(p.y) {}
  const point &operator=( const point &p ) {
    if ( &p != this ) {
      x=p.x; y=p.y;
    }
    return *this;
  }
  int operator < (const point& p) const {return x<p.x;}
  point operator - (const point& p) const {return point(x-p.x,y-p.y);}
  point operator + (const point& p) const {return point(x+p.x,y+p.y);}
  point operator / (double f) const {return point(x/f,y/f);}
  double dot(const point& p) const {return (x*p.x+y*p.y);}
#if defined(DEBUG_PRINT)
  void print () const {cout << x << ' '<< y <<endl;}
#endif
  void write(ofstream& f) const {f<<x<<' '<<y<<endl;}
public:
  double x,y;
};

class edge{
  const edge &operator=( const edge & );  // assignment operator
public:
  edge(): from(-1),to(-1),lef(-1),let(-1),ref(-1),ret(-1) {}
#if defined(DEBUG_PRINT)
  void print(const point p[]) const {p[from].print();p[to].print();}
#endif
  void write(ofstream& f,const point p[]) const {p[from].write(f);p[to].write(f);}
  bool visible(const point p[],int i) const {
    //test whether an edge on the hull is visible from a point
    //rely on the fact that a) hull is anticlockwise oriented
    //b)data is positive x ordered

    const double mindistance = 0.0000001;
    if (fabs(p[from].x-p[to].x) < mindistance) return true;
    if (fabs(p[to].y-p[from].y)<mindistance){
      if(p[from].x<p[to].x && p[i].y<p[from].y)return true;
      if(p[from].x>p[to].x && p[i].y>p[from].y)return true;
    }
    if (p[to].y>=p[i].y && p[from].y<=p[i].y && fabs(p[from].y-p[to].y)>mindistance)return true;

    if (p[to].x>p[from].x){
      if(p[i].y<p[from].y+(p[to].y-p[from].y)/(p[to].x-p[from].x)*(p[i].x-p[from].x))return true;
    }else if (p[to].x<p[from].x) {
      if(p[i].y>p[from].y+(p[to].y-p[from].y)/(p[to].x-p[from].x)*(p[i].x-p[from].x))return true;
    }
    return false;
  }
  int swap(int tint,edge e[],const point p[]){

    //edge swapping routine - each edge has four neighbour edges
    //left and attached to from node (lef) right attached to to node (ret) etc.
    //these edges may be oriented so that their from node is that of 
    // the current edge, or not
    //this routine takes the lions share of the CPU - hull construction by comparison
    //takes much less (by about a factor of ten!)
    if (ref==-1 || lef==-1 || let==-1 || ret==-1)return 0;
    //test orientation of left and right edges - store the indices of
    //points that are not part of the current edge
    int leftp,rightp;
    if (e[lef].from==from)leftp=e[lef].to; else leftp=e[lef].from;
    if (e[ref].from==from)rightp=e[ref].to; else rightp=e[ref].from;
    const point 
      p1(p[leftp]-p[from]), p2(p[leftp]-p[to]), 
      p3(p[rightp]-p[from]), p4(p[rightp]-p[to]);
    double dt1=p1.dot(p2);double dt2=p3.dot(p4);
    //only do the square roots if we really need to - saves a bit of time
    if (dt1<0 || dt2<0){
      dt1=dt1/sqrt(p1.dot(p1)*p2.dot(p2));
      dt2=dt2/sqrt(p3.dot(p3)*p4.dot(p4));
      if ((dt1+dt2)<0){
	//now swap the left and right edges of neighbouring edges
	//taking into account orientation
	if (e[ref].from == from){
	  e[ref].lef=lef;
	  e[ref].let=tint;
	}else{
	  e[ref].ref=tint;
	  e[ref].ret=lef;
	}
	if (e[lef].from==from){
	  e[lef].ref=ref;
	  e[lef].ret=tint;
	}else{
	  e[lef].lef=tint;
	  e[lef].let=ref;
	}
	if (e[ret].to==to){
	  e[ret].lef=tint;
	  e[ret].let=let;
	}else{
	  e[ret].ref=let;
	  e[ret].ret=tint;
	}
	if (e[let].to==to){
	  e[let].ref=tint;
	  e[let].ret=ret;
	}else{
	  e[let].lef=ret;
	  e[let].let=tint;
	}
	//change the end-points for the current edge
	to=rightp;
	from=leftp;
	//re-jig the edges
	int rf=ref;
	ref=lef;
	int rt=ret;
	ret=rf;
	int lt=let;
	let=rt;
	lef=lt;
	//examine the neighbouring edges for delauniness recursively - this is
	//a lot more efficient than trying to swap all edges right at the end.
	e[lef].swap(lef,e,p);
	e[let].swap(let,e,p);
	e[ref].swap(ref,e,p);
	e[ret].swap(ret,e,p);
	return 1;
      }
    }
    return 0;
  }
public:
  int from,to;
  int lef,let,ref,ret;
};

class cyclist{
  //a fixed size linked cyclical list using arrays
  //the code here is not yet robust necessarily to list shrinking to zero
  //number of elements
  const cyclist &operator=( const cyclist & );  // assignment operator
  cyclist( const cyclist & );
public:
  cyclist(int s): size(s),prev(0),hole(0),num(0),ejs(0) {
    ejs=new item[size];
    //fill ej array with a set of pointers to the next unfilled location
    for (int i=0;i<size;i++)ejs[i].data=i+1;
    //hole points to the next unfilled location, prev to the location that was last filled
    //we keep track of empty bits of the list using hole to point to an empty slot, and the
    //value ejs[hole] to point to the next empty slot
  }
  ~cyclist(){
    delete [] ejs;
  }
  int getEdge(int list_pos) const {
    assert(list_pos<size);
    return ejs[list_pos].data;
  }
  int delNextPos(int list_pos){
    assert(list_pos<size);
    assert(num!=0);
    ejs[list_pos].data=hole;
    hole=list_pos;
    ejs[ejs[list_pos].prev].next=ejs[list_pos].next;
    ejs[ejs[list_pos].next].prev=ejs[list_pos].prev;
    num--;
    return ejs[list_pos].next;
  }
  int delNextNeg(int list_pos){
    assert(list_pos<size);
    assert(num!=0);
    ejs[list_pos].data=hole;
    hole=list_pos;
    ejs[ejs[list_pos].prev].next=ejs[list_pos].next;
    ejs[ejs[list_pos].next].prev=ejs[list_pos].prev;
    num--;
    return ejs[list_pos].prev;
  }
  int getNextPos(int list_pos) const {
    assert(list_pos<size);
    return ejs[list_pos].next;
  }
  int getNextNeg(int list_pos){
    assert(list_pos<size);
    return ejs[list_pos].prev;
  }
  void add(int ej){
    //build hull from scratch in numerical order - we assume you got the orientation
    //right!! (anti-clockwise)
    assert(hole<size);
    int n=ejs[hole].data;
    ejs[hole].data=ej;
    //prev stores the location of the place in the array that was most recently filled
    ejs[prev].next=hole;
    //rev is the set of backward pointers
    ejs[hole].prev=prev;
    //the list is cyclic
    ejs[hole].next=0;
    ejs[0].prev=hole;
    prev=hole;
    hole=n;num++;
  }
  int addBefore(int a, int ej){
    assert(a<size);
    //first check for the empty list
    if (num ==0){add(ej);return prev;}
    //otherwise add on before the specified position, using the empty storage slot
    int n=ejs[hole].data;
    ejs[hole].prev=ejs[a].prev;
    ejs[hole].next=a;
    ejs[ejs[a].prev].next=hole;
    ejs[a].prev=hole;
    ejs[hole].data=ej;
    prev=hole;
    hole=n;
    num++;
    //return the value that hole had at the start of the method 
    return prev;
  }
  int addAfter(int a,int ej){
    assert(a<size);
    //first check for the empty list
    if (num ==0){add(ej);return prev;}
    //otherwise add on after the specified position, using the empty storage slot
    int n=ejs[hole].data;
    ejs[hole].next=ejs[a].next;
    ejs[ejs[a].next].prev=hole;
    ejs[a].next=hole;
    ejs[hole].prev=a;
    ejs[hole].data=ej;
    prev=hole;
    hole=n;
    num++;
    //return the value that hole had at the start of the method 
    return prev;
  }
#if defined(DEBUG_PRINT)
  void print() const {int j=ejs[0].next;for (int i=0;i<num;i++){cout<<ejs[j].data<<endl;j=ejs[j].next;}}
#endif
private:
  const int size; 
  int *order,*rev,prev,hole,num;
  struct item{
    int next,prev,data;
  };
  item* ejs;
};

void triangulate(int npoints,const point p[]){

  //convex hull is a cyclical list - it will consist of anticlockwise
  //ordered edges - since each new point adds at most 1 extra edge (nett)
  //to the hull, there are at most npoints edges on the hull
  cyclist hull(npoints);
  
  //and the edges - there are at most three edges per point
  const long nn=3*npoints;
  edge* edges;
  edges=new edge[nn];

  //make first three edges  - these will form the initial convex hull
  //make sure orientation is anticlockwise
  //modify for equal x coords!!!!!
  if (p[2].y>p[0].y+(p[1].y-p[0].y)*(p[2].x-p[0].x)/(p[1].x-p[0].x)){
    edges[0].from=0;
    edges[0].to=1;
    edges[1].from=1;
    edges[1].to=2;
    edges[2].from=2;
    edges[2].to=0;
  }
  else{
    edges[0].from=0;
    edges[0].to=2;
    edges[1].from=2;
    edges[1].to=1;
    edges[2].from=1;
    edges[2].to=0;
  }
  //make left edges
  edges[0].lef=2;
  edges[0].let=1;
  edges[1].lef=0;
  edges[1].let=2;
  edges[2].lef=1;
  edges[2].let=0;
  // add the edges to the hull in order
  // get upper and lower edges as indices into the hull
  // 'cos hull is cyclic, we don't need to bother if these are visible
  // but they do need to be oriented so that upper edge is further round the 
  // hull in a positive direction than lower_hull_pos
  int start=hull.addAfter(0,0);
  int lower_hull_pos=hull.addAfter(start,1); 
  int upper_hull_pos=hull.addAfter(lower_hull_pos,2);
  // loop through the remaining points adding edges to the hull
  int next_edge=3;
  int count,saved_edge;
  
  for (int i=3;i<npoints;i++){
    saved_edge=-1;
    //go round the hull looking for visible edges - we need to go round
    //in two directions from the current upper and lower edges
    //first set up the new edge that joins to the point coincident
    //between upper and lower edge
    if(edges[hull.getEdge(upper_hull_pos)].visible(p,i)){
      //make new edge - from and to nodes preserve hull orientation
      edges[next_edge].from=edges[hull.getEdge(upper_hull_pos)].from;
      edges[next_edge].to=i;
      //save hull position for possible later use
      saved_edge=next_edge;
      //connectivity for swapping - we know the ID of the
      //next edge to be created since upper hull is visible
      edges[next_edge].lef=hull.getEdge(upper_hull_pos);
      edges[next_edge].let=next_edge+1;
      next_edge++;
    }
    else{
      //we can't see the upper edge - can we see the lower one?
      if(!edges[hull.getEdge(lower_hull_pos)].visible(p,i)){
	//can't see the upper or lower edge - chose a bad initial state! go round
	//the hull a bit.Upper hull will still be invisible (its what used to be lower hull)
	if (i==3){
	  lower_hull_pos=hull.getNextPos(upper_hull_pos);
	  upper_hull_pos=hull.getNextPos(lower_hull_pos);
	}else{
	  //or else its an error!
	  cout<<"Triangulate: Can't see the hull from the new point!? number is "<<i<<endl;
	  exit(1);
	}
      }
      edges[next_edge].from=i;
      edges[next_edge].to=edges[hull.getEdge(lower_hull_pos)].to;

      //connectivity for swapping - we know the ID of the
      //next edge but one to be created since upper hull is *not* visible
      edges[next_edge].let=hull.getEdge(lower_hull_pos);
      edges[next_edge].lef=next_edge+1;
      next_edge++;
    }
    //now we need to add the upper hull edges - drop through to set edge made above
    //to the new upper edge if upper edge not visible
    count=0;
    int h;
    while (edges[hull.getEdge(upper_hull_pos)].visible(p,i)){
      h=hull.getEdge(upper_hull_pos);
      edges[next_edge].from=i;
      edges[next_edge].to=edges[h].to;
      //if we got here, the upper hull is visible, so we know which way the edges point
      //set the left and right neighbour edges
      if (count!=0){
	//if we're on the second time round the loop, the edge made in
	// the previous pass needs to be connected on its right side
	edges[next_edge-1].ref=next_edge;
	edges[next_edge-1].ret=h;
      }
      count++;
      edges[h].ref=next_edge-1;
      edges[h].ret=next_edge;
      edges[next_edge].let=h;
      edges[next_edge].lef=next_edge-1;
      //check the hull edge's delauniness
      edges[h].swap(h,edges,p);
      next_edge++;
      //delete upper edge from the hull
      //and go round the hull in the positive direction
      upper_hull_pos=hull.delNextPos(upper_hull_pos);
    }
    //we drop through to here if upper edge is not visible
    //add edge to the hull
    //set the new upper edge to the most recently created upper edge
    upper_hull_pos=hull.addBefore(upper_hull_pos,next_edge-1);
    //now we need to add the lower hull edges
    while (edges[hull.getEdge(lower_hull_pos)].visible(p,i)){
      h=hull.getEdge(lower_hull_pos);
      //upper hull was not visible
      if (saved_edge==-1){
	//after this pass through the loop, always do the else clause
	saved_edge=next_edge-1;
      }else{
	//connect right side of previously created edge
	edges[saved_edge].ref=h;
	edges[saved_edge].ret=next_edge;
      }
      edges[next_edge].to=i;
      edges[next_edge].from=edges[h].from;
      edges[h].ret=saved_edge;
      edges[h].ref=next_edge;
      edges[next_edge].lef=h;
      edges[next_edge].let=saved_edge;
      //swap it if it needs it
      edges[h].swap(h,edges,p);
      //keep the edge for below - this is necessary in case the upper hull wasn't visible
      saved_edge=next_edge;
      next_edge++;
      //delete lower edge from the hull
      //and go round the hull in the negative direction
      lower_hull_pos=hull.delNextNeg(lower_hull_pos);
    }
    //add edge to the hull
    //set the new lower edge to the most recently created lower edge
    //if no lower hull pos was visible, use the saved edge from the upper pos earlier on
    lower_hull_pos=hull.addAfter(lower_hull_pos,saved_edge);
  }

  {
    ofstream file("triggy");
    int i=0;
    while(edges[i].from != -1 ){edges[i].write(file,p);i++;}
  }
#if 0
  {
    int i=0;
    while(edges[i].from != -1 ){edges[i].print(p);i++;}
    cout << "npoint=" << npoints << " nedge=" << i-1 << endl;
  }
#endif

  delete [] edges;
}
 
#include "heapsort.h"

int main(){
  const int n=100;
  const long npoints=n*n;
  //set up the point array
  point *p = new point[npoints];
  //make a set of points perturbed from a uniform grid
  for (int i=0;i<n;i++){
    for(int j=0;j<n;j++){
      p[i+j*n].x=double(i)+double(rand())/RAND_MAX*n*1.e-3;
      p[i+j*n].y=double(j)+double(rand())/RAND_MAX*n*1.e-3;
    } 
  }
  //sort the points - note that the point class defines the
  // < operator so that the sort is on the x co-ordinate
  //array p will be replaced with the array sorted in x
  heapsort(npoints,p);

  time_t t1 = time(NULL);
  clock_t tick1 = clock();

  //triangulate the set of points
  triangulate(npoints,p);

  time_t t2 = time(NULL);
  clock_t tick2 = clock();
  cout << "elapsed time (time) = " << difftime(t2,t1) << " s" << endl;
  cout << "elapsed time (clock)= " << (double)(tick2-tick1)/CLOCKS_PER_SEC << " s" << endl;
  
  delete [] p;
}
