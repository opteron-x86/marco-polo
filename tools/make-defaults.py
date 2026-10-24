#!/usr/bin/env python3
"""Author the bundled balanced starting economy and classification rules."""
import json
from pathlib import Path
goods=[('grain','Grain',5),('food','Food',8),('meat','Meat and fish',10),('drink','Drink',15),('timber','Timber',5),('leather','Hides and leather',15),('iron','Iron',12),('steel','Steel',20),('silver','Silver',50),('gold','Gold ore and ingots',100),('corundum','Corundum',40),('dwarven','Dwarven metal',30),('orichalcum','Orichalcum',45),('moonstone','Moonstone',60),('quicksilver','Quicksilver',60),('malachite','Malachite',70),('ebony','Ebony',120),('ingredients','Alchemy ingredients',12),('medicine','Potions and poisons',40),('weapons','Weapons',60),('armor','Armor',90),('gems','Gems',100),('soul_gems','Soul gems',80),('books','Books and scrolls',30)]
markets=[('whiterun','Whiterun','016772',.25,{'grain':1.7,'food':1.4,'leather':1.3,'iron':1.1}),('reach','The Reach','016769',.35,{'iron':1.8,'silver':2.0,'gold':1.5,'dwarven':1.6,'grain':.4}),('rift','The Rift','01676C',.2,{'timber':1.8,'drink':1.9,'ingredients':1.4,'ebony':1.3}),('haafingar','Haafingar','016770',.5,{'books':1.3,'medicine':1.3,'food':.7,'silver':.6}),('eastmarch','Eastmarch','01676A',.6,{'iron':1.4,'corundum':1.5,'quicksilver':1.3,'grain':.5}),('falkreath','Falkreath','01676F',.25,{'timber':1.9,'leather':1.6,'ingredients':1.4}),('pale','The Pale','01676D',.75,{'iron':1.6,'quicksilver':1.7,'meat':1.3,'food':.5}),('hjaalmarch','Hjaalmarch','01676E',.55,{'ingredients':1.8,'timber':1.2,'meat':1.4,'grain':.5}),('winterhold','Winterhold','01676B',.85,{'soul_gems':1.8,'books':1.8,'medicine':1.3,'grain':.35,'food':.4}),('solstheim','Solstheim','Dragonborn.esm|016E2A',.55,{'ebony':1.9,'ingredients':1.4,'grain':.35,'timber':.5})]
goods.append(('housewares','Household goods',12))
housewares_output={'whiterun':1.25,'reach':1.1,'rift':1.35,'haafingar':1.2,'eastmarch':1.1,'falkreath':.85,'pale':.7,'hjaalmarch':.65,'winterhold':.4,'solstheim':.55}
for key,name,loc,winter,bias in markets:
    bias['housewares']=housewares_output[key]
# Production and local costs establish useful export/import specializations.
# Warehouse scarcity still moves prices around these starting values.
trade_output={
    'whiterun':{'grain':1.9,'food':1.8,'iron':1.4,'steel':1.8,'weapons':2.2,'armor':1.7,'drink':1.6,'housewares':1.35},
    'reach':{'silver':2.4,'gold':2.2,'dwarven':2.1,'orichalcum':1.8,'moonstone':2.2,'gems':1.8,'weapons':1.3,'armor':1.3,'drink':.6,'timber':.65,'food':.55,'books':.65},
    'rift':{'drink':2.4,'weapons':.8,'armor':.8,'housewares':1.5,'gems':.5},
    'haafingar':{'drink':.65,'weapons':.6,'armor':.65,'timber':.65,'silver':.45},
    'eastmarch':{'weapons':1.8,'armor':1.6,'steel':1.7,'corundum':2.0,'malachite':2.2,'drink':.9,'books':.6},
    'falkreath':{'leather':2.2,'weapons':.75,'armor':.75,'drink':.65,'medicine':.55},
    'pale':{'quicksilver':2.2,'weapons':.5,'armor':.65,'drink':.4,'timber':.65,'medicine':.6},
    'hjaalmarch':{'weapons':.6,'armor':.55,'drink':.55,'medicine':.7},
    'winterhold':{'weapons':.3,'armor':.35,'drink':.3,'silver':.5,'timber':.35,'housewares':.35,'soul_gems':2.2,'books':2.2},
    'solstheim':{'ebony':2.4,'weapons':.5,'armor':.55,'drink':.65,'silver':.6},
}
demand_scale={
    'haafingar':{'silver':1.5,'drink':1.5,'weapons':1.25},
    'pale':{'drink':1.25,'medicine':1.25},
    'winterhold':{'weapons':1.25,'drink':1.25,'timber':1.25},
}
for key,name,loc,winter,bias in markets:
    bias.update(trade_output[key])
commodity=[dict(id=i,name=n,referenceValue=v,elasticity=.3 if i in ('grain','food','meat') else .55,shelfLife=80 if i in ('food','meat') else 0,seasonal=i in ('grain','food','meat','timber'),military=i in ('iron','steel','medicine','weapons','armor')) for i,n,v in goods]
defs=[]
for key,name,loc,winter,bias in markets:
    demand={i:30 if i in ('grain','food','meat') else 12 if v<40 else 4 if v<90 else 2 for i,n,v in goods}
    for good,factor in demand_scale.get(key,{}).items():
        demand[good]*=factor
    output={i:round(n*bias.get(i,1.08),2) for i,n in demand.items()}
    prices={i:round(max(.6,min(2.2,(1.08/bias.get(i,1.08))**1.2)),3) for i,n,v in goods}
    defs.append(dict(id=key,name=name,location=loc if '|' in loc else 'Skyrim.esm|'+loc,population=1,winterSeverity=winter,demand=demand,output=output,priceFactors=prices))
edges=[('whiterun','reach',3),('whiterun','rift',3),('whiterun','eastmarch',3),('whiterun','pale',2),('whiterun','falkreath',2),('reach','haafingar',3),('reach','falkreath',3),('haafingar','hjaalmarch',2),('hjaalmarch','pale',2),('pale','winterhold',2),('winterhold','eastmarch',2),('eastmarch','rift',2),('eastmarch','solstheim',4)]
routes=[]
for a,b,days in edges:
    for f,t in [(a,b),(b,a)]:routes.append(dict(id=f+'-'+t,**{'from':f,'to':t},days=days,capacity=100,cost=.06,risk=.025))
data=dict(schema=1,policy=dict(targetDays=10,minMultiplier=.4,maxMultiplier=3,dailyAdjustment=.12,merchantTurnover=.08,treasury=20000,barterExponent=.2,minimumBarterMargin=.05),commodities=commodity,markets=defs,routes=routes,recipes=[dict(market=m['id'],output='steel',amount=2,inputs={'iron':3}) for m in defs])
root=Path(__file__).resolve().parents[1]/'package/SKSE/Plugins/MarcoPolo'
(root/'economy.json').write_text(json.dumps(data,indent=2)+'\n')
# Exact form mappings work without editor-ID persistence or po3 Tweaks.
forms={'iron':['Skyrim.esm|05ACE4','Skyrim.esm|071CF3'],'steel':['Skyrim.esm|05ACE5'],'silver':['Skyrim.esm|05ACE3','Skyrim.esm|05ACDF'],'gold':['Skyrim.esm|05AD9E','Skyrim.esm|05ACDE'],'corundum':['Skyrim.esm|05AD93','Skyrim.esm|05ACDB'],'dwarven':['Skyrim.esm|0DB8A2'],'orichalcum':['Skyrim.esm|05AD99','Skyrim.esm|05ACDD'],'moonstone':['Skyrim.esm|05AD9F','Skyrim.esm|05ACE0'],'quicksilver':['Skyrim.esm|05ADA0','Skyrim.esm|05ACE2'],'malachite':['Skyrim.esm|05ADA1','Skyrim.esm|05ACE1'],'ebony':['Skyrim.esm|05AD9D','Skyrim.esm|05ACDC'],'leather':['Skyrim.esm|0DB5D2','Skyrim.esm|0800E4'],'timber':['Skyrim.esm|06F993'],'grain':['Skyrim.esm|04B0BA']}
rules=dict(forms={f:c for c,fs in forms.items() for f in fs},keywords={'VendorItemFood':'food','VendorItemFoodRaw':'meat','VendorItemPotion':'medicine','VendorItemPoison':'medicine','VendorItemIngredient':'ingredients','VendorItemGem':'gems','VendorItemAnimalHide':'leather','VendorItemAnimalPart':'leather','VendorItemBook':'books','VendorItemScroll':'books','VendorItemSoulGem':'soul_gems'},types={'Weapon':'weapons','Armor':'armor','Ingredient':'ingredients','AlchemyItem':'medicine','SoulGem':'soul_gems','Book':'books','Scroll':'books'},excluded=['Skyrim.esm|00000F','Skyrim.esm|00000A'])
rules['forms'].update({'Skyrim.esm|065C9B':'meat','Skyrim.esm|034C5E':'drink'})
# Vanilla, Hearthfire and Dragonborn alcoholic drinks. Quest instances remain
# protected by the runtime. These records were checked against the game plugins.
drinks={
    'Skyrim.esm':[0x34C5E,0x34C5D,0x3133B,0x3133C,0xC5348,0xC5349,
                  0x2C35A,0xF693F,0x508CA,0x85368,0xB91D7,0x107A8A,
                  0x555E8,0x1895F,0xF257E,0x9380D,0xD055E,0x65C37,
                  0x65C38,0x65C39,0x36D53],
    'HearthFires.esm':[0x3535,0x3536],
    'Dragonborn.esm':[0x3572F,0x320DF,0x24E0B,0x248CC,0x207E6,0x207E5,0x248CE],
}
for plugin,ids in drinks.items():
    rules['forms'].update({f'{plugin}|{local:06X}':'drink' for local in ids})
# Optional keyword providers require no plugin master or runtime dependency.
for keyword in ('OCF_AlchDrinkAlcohol','VendorItemDrinkAlcohol',
                'VendorItemDrinkAlcoholModerate','VendorItemDrinkAlcoholStrong'):
    rules['keywords'][keyword]='drink'
rules['keywords']['VendorItemClutter']='housewares'
# Vanilla silverware lacks the clutter keyword. Explicitly include these goods
# and the untagged rugs from Hearthfire Extended; optional forms add no dependency.
housewares={'Skyrim.esm':[0x98623,0x98624,0x98625,0x98626,0x98627,0xE42DF,0xE42E0,0xE42E1,0x34CD6,0x33760],
            'hearthfireextended.esp':[0x1D3EBE,*range(0x1D3EC0,0x1D3EC7)]}
for plugin,ids in housewares.items():
    rules['forms'].update({f'{plugin}|{local:06X}':'housewares' for local in ids})
rules['excluded'] += [f'Marco Polo - Missives.esp|{0x800+i:06X}' for i in range(18)]
(root/'classification.json').write_text(json.dumps(rules,indent=2)+'\n')
