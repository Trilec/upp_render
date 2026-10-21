#include <RenderVector/RenderVector.h>
#include <RenderSoftware/RenderSoftware.h>
#include <cmath>
using namespace Upp;
static bool Check(bool condition,const char *message) {
	if(!condition) Cout()<<"FAIL: "<<message<<EOL;
	return condition;
}
CONSOLE_APP_MAIN
{
	bool ok=true; int cases=0; const Size size(96,96);
	for(int kind=0;kind<10;++kind) {
		Function<void(Painter&)> paint=[=](Painter& p) {
			if(kind==0) p.RoundedRectangle(8.5,9.5,67,61,12).Fill(Color(70,150,210)).Stroke(2,White());
			if(kind==1) {
				p.Begin(); p.Translate(7,4).Scale(.9,1.1);
				p.Move(10,14).RelLine(50,0).RelQuadratic(12,18,0,40).RelLine(-50,0).Close();
				p.Fill(White()).Stroke(3,Color(40,160,220)); p.End();
			}
			if(kind==2) {
				p.Move(8,20).Cubic(12,2,25,2,30,20).Cubic(Pointf(45,38),Pointf(60,20));
				p.Quadratic(75,3,82,30).Quadratic(Pointf(68,60));
				p.LineCap(LINECAP_ROUND).LineJoin(LINEJOIN_ROUND).Dash("5 3").Stroke(3,White());
			}
			if(kind==3) p.Move(74,45).Arc(45,45,29,22,0,2*M_PI).Close().Fill(Color(150,80,210));
			if(kind==4) {
				p.Move(12,50).SvgArc(Pointf(33,21),.3,false,true,Pointf(78,50));
				p.Stroke(3,White());
			}
			if(kind==5) {
				p.Rectangle(8,8,75,75);
				p.ColorStop(.4,Color(50,180,110));
				p.Fill(Pointf(8,8),Color(10,30,220),Pointf(83,83),Color(230,80,30));
			}
			if(kind==6) {
				p.Begin(); p.Opacity(.45);
				p.Rectangle(9,10,30,25).Fill(Color(20,110,230));
				p.Begin(); p.Opacity(.6).Rectangle(20,22,30,30).Fill(Color(180,70,20)).End();
				p.End();
				p.Rectangle(57,12,20,45).Fill(170*Color(190,100,45));
			}
			if(kind==7) {
				p.Begin(); p.Translate(4,5).Rectangle(10,10,45,38).Clip();
				p.Rectangle(0,0,90,90).Fill(Color(60,120,200)); p.End();
				p.Rectangle(70,70,10,10).Fill(White());
			}
			if(kind==8) {
				Xform2D m=Xform2D::Scale(45,30)*Xform2D::Rotation(.3)*Xform2D::Translation(30,35);
				p.Rectangle(10,10,70,70).Fill(Color(20,50,200),Color(210,170,40),m,GRADIENT_REFLECT);
			}
			if(kind==9) {
				Xform2D m=Xform2D::Scale(37,25)*Xform2D::Rotation(.4)*Xform2D::Translation(44,44);
				p.Rectangle(6,6,80,80).Fill(Pointf(.1,-.2),Color(10,70,210),Color(220,140,30),m);
			}
		};
		UiDisplayList list; String error;
		if(!Check(RecordUiPainter(size,paint,list,error),"semantic Painter records")) {
			Cout()<<"case="<<kind<<" "<<error<<EOL; ok=false; continue;
		}
		for(int i=0;i<list.GetCount();++i)
			ok &= Check(list[i].type!=UiDisplayOpType::DrawImage,"vector recording creates no image pixels");
		ImagePainter expected(size,MODE_ANTIALIASED); expected.Clear(RGBAZero()); paint(expected);
		ImagePainter actual(size,MODE_ANTIALIASED); actual.Clear(RGBAZero());
		SoftwareUiRenderer renderer; ok &= Check(renderer.Replay(list,actual),"recorded vector replays");
		Image a=actual.GetResult(), e=expected.GetResult(); int maximum=0; double mean=0;
		for(int y=0;y<size.cy;++y) for(int x=0;x<size.cx;++x) {
			RGBA aa=a[y][x], ee=e[y][x];
			for(int delta:{abs((int)aa.r-ee.r),abs((int)aa.g-ee.g),abs((int)aa.b-ee.b),abs((int)aa.a-ee.a)}) {
				maximum=max(maximum,delta); mean+=delta/(96.0*96*4);
			}
		}
		Cout()<<"case="<<kind<<" max="<<maximum<<" mean="<<mean<<EOL;
		ok &= Check(maximum<=3 && mean<=.3,"semantic recording preserves Painter pixels");
		++cases;
	}
	{
		UiDisplayList list; String error;
		ok &= Check(RecordUiPainter(size,[](Painter& p) {
			p.Move(2,2).Line(80,10).Line(30,80).Close().Clip();
		},list,error) && error.IsEmpty(),"nonrectangular clips record without CPU masks");
		ok &= Check(!RecordUiPainter(size,[](Painter& p) { p.End(); },list,error),
		            "unbalanced state fails explicitly");
		ok &= Check(!RecordUiPainter(size,[](Painter& p) { p.BeginMask(); },list,error),
		            "unsupported mask fails explicitly");
	}
	Cout()<<"semantic_cases="<<cases<<EOL;
	if(ok && cases==10) Cout()<<"RenderSemanticPainterTest passed"<<EOL;
	else SetExitCode(1);
}
