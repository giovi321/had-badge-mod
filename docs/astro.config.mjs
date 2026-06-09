import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';

export default defineConfig({
  site: 'https://giovi321.github.io',
  base: '/had-badge-mod',
  integrations: [
    starlight({
      title: 'Communicator Badge',
      description: 'C / ESP-IDF Meshtastic firmware for the Hackaday Communicator badge',
      social: [
        {
          icon: 'github',
          label: 'GitHub',
          href: 'https://github.com/giovi321/had-badge-mod',
        },
      ],
      editLink: {
        baseUrl: 'https://github.com/giovi321/had-badge-mod/edit/main/docs/',
      },
      sidebar: [
        { label: 'Home', link: '/' },
        {
          label: 'Getting started',
          items: [
            { label: 'Install ESP-IDF', link: '/getting-started/installation/' },
            { label: 'Build and flash', link: '/getting-started/building/' },
            { label: 'First boot', link: '/getting-started/bring-up/' },
          ],
        },
        {
          label: 'Hardware',
          items: [
            { label: 'Board and pins', link: '/hardware/overview/' },
          ],
        },
        {
          label: 'Architecture',
          items: [
            { label: 'Overview', link: '/architecture/overview/' },
            { label: 'Meshtastic stack', link: '/architecture/meshtastic/' },
            { label: 'User interface', link: '/architecture/ui/' },
          ],
        },
        {
          label: 'Apps',
          items: [
            { label: 'Messages', link: '/apps/messages/' },
            { label: 'Nodes', link: '/apps/nodes/' },
            { label: 'Diagnostics', link: '/apps/diagnostics/' },
            { label: 'Settings', link: '/apps/settings/' },
            { label: 'GPS', link: '/apps/gps/' },
            { label: 'Breadcrumbs and Follow', link: '/apps/breadcrumbs/' },
            { label: 'Tracker', link: '/apps/tracker/' },
            { label: 'Packets', link: '/apps/packets/' },
          ],
        },
        {
          label: 'Connectivity',
          items: [
            { label: 'WiFi and web UI', link: '/connectivity/wifi/' },
            { label: 'Bluetooth', link: '/connectivity/bluetooth/' },
          ],
        },
        {
          label: 'Development',
          items: [
            { label: 'Host tests', link: '/development/host-tests/' },
            { label: 'Conventions', link: '/development/conventions/' },
            { label: 'Troubleshooting', link: '/development/troubleshooting/' },
          ],
        },
      ],
    }),
  ],
});
