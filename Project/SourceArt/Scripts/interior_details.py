"""Original small-scale furnishing details used by build_interiors.py.

Geometry stays in the room's shared metre frame; large objects register their
footprints with the same collision plan as the existing furniture.
"""

def softbox(b,p,d,colour=LINEN,power=.45):
    g['craft'].cushion(b,p,d,colour)

def cup(b,x,y,z,colour=IVORY):
    g['craft'].cup(b,x,y,z,colour)

def pastry(b,x,y,z,yaw=0):
    points=[(x+math.cos(yaw)*math.sin(t/16*math.pi)*.18+math.sin(yaw)*(t/16-.5)*.30,
             y+math.sin(yaw)*math.sin(t/16*math.pi)*.18-math.cos(yaw)*(t/16-.5)*.30,
             z+.035+.045*math.sin(t/16*math.pi)) for t in range(17)]
    b.tube(points,[.013+.052*math.sin(t/16*math.pi)**.6 for t in range(17)],'Paint',10,(.70,.31,.065,1))
    for t in [3,6,9,12]:
        pp=points[t];box(b,(pp[0],pp[1],pp[2]+.048),(.025,.09,.009),'Paint',(.92,.54,.17,1),.003,yaw)

def round_table(b,x,y,r=.65,z=.74,colour=LIGHT_OAK):
    b.lathe([(0,0),(r,0),(r+.025,.025),(r,.075),(0,.075)],(x,y,z),'Timber',48,colour)
    b.lathe([(0,0),(.30,0),(.32,.045),(.12,.11),(.065,z),(0,z)],(x,y,0),'Bronze',32,GOLD)

def enhance_room(b,kind):
    if kind==0:
        for x in [5.10,6.0,6.9]:softbox(b,(x,-12.97,.57),(.84,.67,.23),(.75,.53,.32,1))
        for x,col in [(5.03,TEAL),(6.65,(.55,.16,.065,1))]:
            softbox(b,(x,-12.69,.94),(.57,.24,.52),col)
            box(b,(x,-12.83,.94),(.018,.014,.017),'Bronze',GOLD,.003)
        cup(b,5.68,-13.77,.49);box(b,(5.88,-13.85,.487),(.30,.22,.05),'Paper',IVORY,.01)
        box(b,(4.64,-12.86,1.00),(.40,.90,.065),'Cloth',TEAL,.015)
        for j in range(9):beam(b,(4.46+j*.045,-13.29,1.00),(4.46+j*.045,-13.32,.77),.004,'Cloth',IVORY)
        round_table(b,2.4,-13.12,.54,.70);vase(b,2.4,-13.12,.79)
        solid(kind,(2.4,-13.12,.40),(1.15,1.15,.80))
    elif kind==1:
        for x in [6.08,7.12]:softbox(b,(x,-12.84,.79),(.88,.48,.20),IVORY)
        g['craft'].drape(b,(6.6,-13.80,.75),2.30,1.65,(.44,.12,.075,1),drop=.18)
        softbox(b,(6.6,-13.21,.87),(.72,.26,.23),TEAL)
        box(b,(3.10,-12.60,.75),(1.15,.62,.075),colour=LIGHT_OAK)
        for x in [2.64,3.56]:box(b,(x,-12.60,.36),(.07,.47,.72))
        art(b,3.10,-12.23,1.90,.8,1.25);vase(b,3.33,-12.65,.80)
        solid(kind,(3.10,-12.60,.41),(1.15,.64,.82))
    elif kind==2:
        for j in range(5):
            box(b,(6.7+j*.038,-12.59,.96),(.025,.05,.25),'Paint',[(.70,.17,.05,1),TEAL][j%2],.005)
        b.lathe([(0,0),(.085,0),(.085,.12),(0,.12)],(6.8,-12.58,.81),'Ceramic',24,IVORY)
        for j in range(3):box(b,(5.9,-13.06,.84+j*.042),(.35,.25,.037),'Paint',[TEAL,IVORY,(.57,.17,.06,1)][j],.007)
        # Open book has raised pages and a narrow contrasting spine.
        for sign in [-1,1]:box(b,(6.35+sign*.15,-12.99,.84),(.29,.35,.026),'Paper',IVORY,.008,sign*.10)
        plant(b,8.27,-12.87,1.8,403);solid(kind,(8.27,-12.87,.30),(.65,.65,.6))
    elif kind==3:
        for x in [5.8,7.0]:
            for y in [-13.47,-13.99]:
                cup(b,x+.16,y,.88);box(b,(x-.25,y,.868),(.10,.24,.015),'Cloth',TEAL,.003)
                beam(b,(x-.16,y-.09,.877),(x-.16,y+.09,.877),.008)
        for j in range(5):
            b.lathe([(0,0),(.08,0),(.09,.13),(.06,.20),(0,.21)],(1.1+(j%2)*.27,-13.2+(j//2)*.25,1.03),'Ceramic',24,
                    [TEAL,(.69,.26,.10,1),IVORY][j%3])
    elif kind==5:
        cup(b,7.82,-12.78,.85);plant(b,8.48,-13.75,1.1,405);solid(kind,(8.48,-13.75,.30),(.64,.64,.60))

def new_room(b,kind):
    if kind==6:
        # Curved cafe counter, pastry display, espresso machine and window table.
        box(b,(5.85,-12.85,.47),(3.85,1.03,.94),'Paint',TEAL,.10)
        box(b,(5.85,-12.85,.98),(4.02,1.16,.11),'Ceramic',IVORY,.05)
        for j in range(28):box(b,(3.99+j*.138,-13.395,.48),(.055,.037,.77),colour=LIGHT_OAK,bevel=.013)
        solid(kind,(5.85,-12.85,.525),(4.02,1.18,1.05))
        box(b,(6.65,-12.67,1.28),(.90,.55,.47),'Bronze',GOLD,.07)
        for x in [6.42,6.88]:
            beam(b,(x,-12.96,1.27),(x,-13.05,1.17),.019);cup(b,x,-13.03,1.06)
            b.lathe([(0,0),(.045,0),(.045,.035),(0,.035)],(x,-12.66,1.53),'Ceramic',24,IVORY)
        for j in range(3):
            xx=4.37+j*.38;pastry(b,xx,-13.0,1.06,j*.8)
            b.lathe([(0,0),(.21,0),(.23,.02),(0,.03)],(xx,-13,1.042),'Ceramic',32,IVORY)
        for j in range(6):cup(b,4.1+j*.28,-12.57,1.08,[IVORY,TEAL][j%2])
        art(b,5.9,-12.22,2.25,2.6,.95)
        round_table(b,7.25,-16.2,.61,.72);vase(b,7.25,-16.2,.80)
        for x,yaw in [(6.20,-math.pi/2),(8.3,math.pi/2)]:chair(b,x,-16.2,yaw,TEAL)
        solid(kind,(7.25,-16.2,.425),(1.3,1.3,.85))
        for x in [6.2,8.3]:solid(kind,(x,-16.2,.5),(.65,.65,1))
        plant(b,.65,-12.85,1.65,411);solid(kind,(.65,-12.85,.3),(.65,.65,.6))
        bookcase(b,1.6,-12.39,1.4,1.45,412)
        solid(kind,(1.6,-12.55,.8),(1.55,.66,1.6))
    elif kind==7:
        # A lush greenhouse with distinct tiers, terracotta pots and a trellis.
        for j,(x,y,size) in enumerate([(1.,-12.8,2.0),(2.0,-12.8,1.5),(3.0,-12.8,2.2),(7.1,-12.85,2.0),
                                      (8.2,-13.5,2.5),(8.25,-14.5,1.4),(1.0,-14.1,1.5)]):
            plant(b,x,y,size,420+j);solid(kind,(x,y,.3),(.66,.66,.6))
            for a in range(5):
                t=a*math.tau/5;b.leaf((x+math.cos(t)*.20,y+math.sin(t)*.20,.85+size*.28),.80,.40,t,.6,'Foliage',(.08,.36,.095,1))
        for y in [-12.5,-13.25]:
            beam(b,(.45,y,2.95),(8.55,y,2.95),.035,'Timber',LIGHT_OAK)
        for x in [.45,4.5,8.55]:beam(b,(x,-12.5,0),(x,-12.5,3.10),.045,'Timber',LIGHT_OAK)
        for j in range(14):
            x=.8+j*.53;beam(b,(x,-12.3,.8),(x,-12.3,2.9),.018,'Timber',LIGHT_OAK)
        vine=[(.95,-12.75,.46)]+[(.6+i*.38,-12.57,2.5+math.sin(i*.65)*.32) for i in range(20)]+[(7.1,-12.85,.46)]
        b.tube(vine,[.018]*len(vine),'Bark',8,(.10,.19,.045,1))
        for i in range(20):
            x=.6+i*.38;z=2.5+math.sin(i*.65)*.32
            b.leaf((x,-12.57,z),.44,.22,i*.63,.2,'Foliage',(.13,.42,.14,1))
        box(b,(5.4,-13.15,.37),(2.5,.75,.72),colour=LIGHT_OAK)
        box(b,(5.4,-13.15,.78),(2.7,.9,.15),'Cloth',TEAL,.05)
        softbox(b,(4.6,-12.97,1.02),(.64,.23,.48),(.70,.24,.07,1))
        solid(kind,(5.4,-13.15,.48),(2.7,.92,.96))
        round_table(b,5.5,-14.55,.50,.48);vase(b,5.5,-14.55,.56)
        solid(kind,(5.5,-14.55,.30),(1.07,1.07,.60))
    elif kind==8:
        # An easel with a relief landscape and tools forms the main focal point.
        for x in [5.5,7.0]:beam(b,(x,-13.7,0),(6.25+(x-6.25)*.4,-13.0,2.65),.045,'Timber',OAK)
        beam(b,(6.25,-12.5,0),(6.25,-13.0,2.7),.042,'Timber',OAK)
        box(b,(6.25,-13.33,1.11),(1.72,.25,.085),colour=LIGHT_OAK)
        art(b,6.25,-13.25,1.9,1.4,1.3)
        solid(kind,(6.25,-13.2,1.1),(1.70,1.15,2.2))
        box(b,(2.15,-12.8,.8),(2.7,.9,.12),colour=LIGHT_OAK)
        for x in [1.08,3.22]:box(b,(x,-12.8,.38),(.09,.66,.76))
        solid(kind,(2.15,-12.8,.44),(2.7,.9,.88))
        for j,col in enumerate([(.70,.07,.025,1),(.90,.52,.045,1),(.05,.31,.54,1),TEAL,IVORY]):
            x=1.2+j*.33
            b.lathe([(0,0),(.075,0),(.075,.20),(.05,.23),(0,.23)],(x,-12.75,.87),'Ceramic',24,col)
            for k in range(2):beam(b,(x,-12.75,1.02),(x+(k-.5)*.08,-12.76,1.38),.008,'Timber',OAK)
        for j in range(4):
            box(b,(8.17,-12.7+j*.24,.92),(.95,.065,1.65),'Cloth',[IVORY,(.45,.15,.065,1),TEAL,IVORY][j],.01,.05*j)
        solid(kind,(8.17,-12.3,.95),(1.02,1.05,1.9))
        chair(b,4.25,-13.9,-.3,TEAL);solid(kind,(4.25,-13.9,.5),(.65,.65,1))
        plant(b,8.25,-16.7,1.6,430);solid(kind,(8.25,-16.7,.3),(.65,.65,.6))
    elif kind==9:
        # Upright piano, 52 ivory keys, raised black keys, bench and record console.
        box(b,(6.2,-12.75,.73),(2.85,.70,1.46),colour=WALNUT,bevel=.06)
        box(b,(6.2,-13.25,.80),(2.96,.67,.12),colour=WALNUT,bevel=.05)
        box(b,(6.2,-12.98,1.08),(2.55,.10,.37),colour=OAK)
        for j in range(52):
            xx=4.86+j*.052
            box(b,(xx,-13.33,.877),(.049,.36,.030),'Ceramic',IVORY,.002)
            if j%7 in (0,1,3,4,5):box(b,(xx+.025,-13.22,.906),(.031,.19,.045),'Paint',(.012,.015,.017,1),.003)
        for x in [4.85,7.55]:box(b,(x,-13.36,.4),(.10,.18,.8),colour=WALNUT)
        solid(kind,(6.2,-13.03,.74),(3.0,1.17,1.48))
        box(b,(6.2,-14.15,.48),(1.12,.47,.12),'Cloth',(.40,.08,.11,1),.07)
        for x in [5.77,6.63]:
            for y in [-14.31,-13.99]:box(b,(x,y,.22),(.055,.055,.44),colour=WALNUT)
        solid(kind,(6.2,-14.15,.29),(1.15,.50,.58))
        for x in [5.96,6.21]:box(b,(x,-13.03,1.28),(.24,.017,.32),'Paper',IVORY,.003)
        box(b,(1.4,-12.80,.44),(1.88,.73,.88),colour=LIGHT_OAK)
        solid(kind,(1.4,-12.8,.48),(1.91,.77,.96))
        box(b,(1.4,-12.80,.93),(.70,.50,.07),'Paint',TEAL,.015)
        b.lathe([(0,0),(.22,0),(.22,.022),(0,.022)],(1.38,-12.81,.969),'Paint',48,(.018,.027,.034,1))
        b.lathe([(0,0),(.075,0),(.075,.007),(0,.007)],(1.38,-12.81,.993),'Paper',32,(.65,.26,.09,1))
        beam(b,(1.67,-12.66,.99),(1.52,-12.91,.99),.012)
        for j in range(9):box(b,(.78+j*.095,-12.69,.39),(.055,.40,.58),'Paint',[TEAL,IVORY,(.55,.15,.04,1)][j%3],.006)
        art(b,6.2,-12.24,2.24,2.15,.92);lamp(b,8.2,-13.5,1.28,True)
        plant(b,8.32,-16.5,1.5,440);solid(kind,(8.32,-16.5,.30),(.66,.66,.60))

def welcoming_entry(b):
    for x in [.94,3.56]:
        box(b,(x,-18.13,1.43),(.15,.18,2.86),colour=LIGHT_OAK,bevel=.025)
        box(b,(x+(-.14 if x<2 else .14),-17.62,1.39),(.13,.9,2.72),'Paint',TEAL,.025)
        for h in [.52,1.75]:box(b,(x+(-.14 if x<2 else .14),-18.079,h),(.08,.024,.65),colour=LIGHT_OAK,bevel=.006)
    box(b,(2.25,-18.13,2.90),(2.78,.20,.14),colour=LIGHT_OAK,bevel=.025)
    # Carved sunburst above an unmistakably open doorway.
    b.arc((2.25,-18.14,2.94),.60,.045,0,math.pi,'Bronze',.055,32)
    for i in range(7):
        a=i*math.pi/6;beam(b,(2.25,-18.15,2.94),(2.25+math.cos(a)*.51,-18.15,2.94+math.sin(a)*.51),.013)
    for x in [.66,3.85]:
        beam(b,(x,-18.06,2.42),(x,-18.35,2.42),.025)
        b.lathe([(0,0),(.12,0),(.16,.10),(.12,.29),(0,.30)],(x,-18.35,2.11),'Glow',28,(1,.61,.22,1))
        b.lathe([(0,0),(.19,0),(.15,.06),(0,.09)],(x,-18.35,2.40),'Bronze',28,GOLD)
    box(b,(2.25,-18.33,.012),(2.2,.58,.024),'Cloth',TEAL,.008)
    for i in range(15):box(b,(1.24+i*.144,-18.33,.027),(.018,.5,.006),'Cloth',IVORY,.001)
