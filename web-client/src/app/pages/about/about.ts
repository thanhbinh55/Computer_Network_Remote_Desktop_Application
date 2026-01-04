import { Component } from '@angular/core';
import { Location } from '@angular/common';
import { CommonModule } from '@angular/common';
@Component({
  selector: 'app-about',
  standalone: true,
   imports: [CommonModule], 
  templateUrl: './about.html',
  styleUrls: ['./about.css']
})
export class AboutComponent {

   lang: 'en' | 'vi' = 'en';
  constructor(private location: Location) {}
  switchLang(lang: 'en' | 'vi') {
    this.lang = lang;
  }
  goBack() {
    this.location.back();
  }
 


}